#include <Arduino.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_MCP23X17.h>
#include <Sonos.h>
#include <vector>
#include "NowPlaying.h"
#include "SpeakerList.h"
#include "secrets.h"
#include "DeviceCache.h"
#include "ButtonHandler.h"
#include "SonosController.h"
#include "DiscoveryManager.h"

#define TFT_CS  D3
#define TFT_DC  D2
#define TFT_RST D1
#define TFT_BL  D6

// MCP23017 pins
#define BTN_UP    13
#define BTN_DOWN  14
#define BTN_CLICK 15
#define BTN_VUP   12
#define BTN_VDOWN 11

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
Adafruit_MCP23X17 mcp;
Sonos sonos;
NowPlaying nowPlaying;
SpeakerList speakerList;
DeviceCache deviceCache;
SonosController sonosController(sonos);
DiscoveryManager discoveryManager(sonos, deviceCache);
ButtonHandler buttons(mcp, BTN_UP, BTN_DOWN, BTN_CLICK, BTN_VUP, BTN_VDOWN);

int selectedIndex = 0;
IPAddress selectedDeviceIP;

// UI tracking
String lastAlbumArtUrl = "";
String lastTitle = "";
String lastArtist = "";
String lastPlaybackState = "";
int lastProgressPercent = -1;
int lastVolume = -1;

enum ScreenState {
    SCREEN_SPEAKER_LIST,
    SCREEN_NOW_PLAYING
};

ScreenState currentScreen = SCREEN_SPEAKER_LIST;
unsigned long lastRefreshTime = 0;
const unsigned long REFRESH_INTERVAL = 2000; // Refresh every 2 seconds on Now Playing

enum WiFiState {
    WIFI_DISCONNECTED,
    WIFI_CONNECTING,
    WIFI_CONNECTED
};

WiFiState wifiState = WIFI_DISCONNECTED;
WiFiState previousWifiState = WIFI_DISCONNECTED;
unsigned long wifiConnectStartTime = 0;
const unsigned long WIFI_TIMEOUT = 30000; // 30 seconds timeout

void startWiFiConnection() {
    WiFi.persistent(true);
    WiFi.mode(WIFI_STA);
#if USE_STATIC_IP
    WiFi.config(STATIC_IP, GATEWAY, SUBNET);
#endif
    WiFi.begin(ssid, password);
    WiFi.setAutoReconnect(true);
    wifiState = WIFI_CONNECTING;
    wifiConnectStartTime = millis();
}

void checkWiFiConnection() {
    if (wifiState == WIFI_CONNECTING) {
        if (WiFi.status() == WL_CONNECTED) {
            wifiState = WIFI_CONNECTED;
            sonos.begin();
        } else if (millis() - wifiConnectStartTime > WIFI_TIMEOUT) {
            wifiState = WIFI_DISCONNECTED;
            WiFi.disconnect();
            delay(100);
            startWiFiConnection();
        }
    }
}

void handleSpeakerListNavigation() {
    if (sonos.isDiscovering()) return;

    const auto& devices = discoveryManager.getDevices();
    int totalItems = devices.size() + 1; // +1 for Scan button

    bool selectionChanged = false;

    if (buttons.upPressed() && selectedIndex > 0) {
        selectedIndex--;
        selectionChanged = true;
    }

    if (buttons.downPressed() && selectedIndex < totalItems - 1) {
        selectedIndex++;
        selectionChanged = true;
    }

    if (buttons.clickPressed()) {
        if (selectedIndex < (int)devices.size()) {
            selectedDeviceIP.fromString(devices[selectedIndex].ip.c_str());
            currentScreen = SCREEN_NOW_PLAYING;

            // Full redraw reset
            lastAlbumArtUrl = lastTitle = lastArtist = lastPlaybackState = "";
            lastProgressPercent = lastVolume = -1;

            nowPlaying.drawStatic();
            nowPlaying.drawSpeakerInfo(devices[selectedIndex].name.c_str());
            lastRefreshTime = 0;
        } else if (wifiState == WIFI_CONNECTED) {
            speakerList.updateHeader("Scanning...");
            discoveryManager.forceRefresh();
            speakerList.updateHeader("Connected");
            speakerList.refreshDevices(discoveryManager.getDevices());
        } else {
            speakerList.updateHeader("WiFi Required");
        }
        return;
    }

    if (selectionChanged) {
        speakerList.setSelectedIndex(selectedIndex);
        speakerList.refreshDevices(devices);
    }
}
void handleNowPlayingNavigation() {
    if (buttons.clickPressed()) {
        sonosController.togglePlayPause(selectedDeviceIP.toString());
        lastRefreshTime = 0;
        return;
    }

    if (buttons.clickLongPressed()) {
        currentScreen = SCREEN_SPEAKER_LIST;
        speakerList.draw(discoveryManager.getDevices());
        return;
    }

    if (buttons.upPressed()) {
        sonosController.next(selectedDeviceIP.toString());
        lastRefreshTime = 0;
    }
    if (buttons.downPressed()) {
        sonosController.previous(selectedDeviceIP.toString());
        lastRefreshTime = 0;
    }

    if (buttons.volUpPressed()) {
        sonosController.volumeUp(selectedDeviceIP.toString());
        lastRefreshTime = 0;
    }
    if (buttons.volDownPressed()) {
        sonosController.volumeDown(selectedDeviceIP.toString());
        lastRefreshTime = 0;
    }
}

void updateNowPlayingScreen() {
    if (wifiState != WIFI_CONNECTED) return;

    if (sonosController.update(selectedDeviceIP.toString())) {
        const auto& data = sonosController.getTrackData();

        if (data.title != lastTitle || data.artist != lastArtist) {
            lastTitle = data.title;
            lastArtist = data.artist;
            nowPlaying.drawTrackInfo(data.title.c_str(), data.artist.c_str(), data.album.c_str());
        }

        int progressPercent = (data.duration > 0) ? (data.position * 100) / data.duration : 0;
        if (progressPercent != lastProgressPercent) {
            lastProgressPercent = progressPercent;
            nowPlaying.drawProgressBar(data.position, data.duration);
        }
        if (data.volume != lastVolume) {
            lastVolume = data.volume;
            nowPlaying.drawVolume(data.volume);
        }
        if (data.playbackState != lastPlaybackState) {
            lastPlaybackState = data.playbackState;
            nowPlaying.drawStatusBar(data.playbackState.c_str());
        }
        
        if (data.albumArtUrl != lastAlbumArtUrl) {
            lastAlbumArtUrl = data.albumArtUrl;
            nowPlaying.drawAlbumArt(data.albumArtUrl.c_str());
        }
    } else {
        nowPlaying.drawStatusBar("Offline");
    }
}

void setup() {
    Serial.begin(115200);
    Serial.println("Sonos Remote Starting...");
    deviceCache.begin();

    if (!mcp.begin_I2C(0x20)) Serial.println("Error: MCP23017 not found!");

    SonosConfig config;
    config.enableLogging = true;
    config.enableVerboseLogging = true;
    sonos.setConfig(config);

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    tft.init(240, 280);
    tft.setRotation(2);

    buttons.begin();
    tft.fillScreen(ST77XX_BLACK);

    discoveryManager.begin();
    discoveryManager.setDiscoveryCallback([](const std::vector<SonosDevice>& devices) {
        if (currentScreen == SCREEN_SPEAKER_LIST) speakerList.refreshDevices(devices);
    });
    speakerList.setSelectedIndex(0);
    speakerList.draw(discoveryManager.getDevices());

    startWiFiConnection();
}

void loop() {
    checkWiFiConnection();
    buttons.update();

    if (currentScreen == SCREEN_SPEAKER_LIST) {
        handleSpeakerListNavigation();
        discoveryManager.update();

        if (wifiState != previousWifiState) {
            previousWifiState = wifiState;
            if (wifiState == WIFI_CONNECTING) speakerList.updateHeader("WiFi: Connecting...");
            else if (wifiState == WIFI_CONNECTED) speakerList.updateHeader("WiFi: Connected");
            else if (wifiState == WIFI_DISCONNECTED) speakerList.updateHeader("WiFi: Failed");
        }
    } else if (currentScreen == SCREEN_NOW_PLAYING) {
        handleNowPlayingNavigation();

        unsigned long currentMillis = millis();
        if (currentMillis - lastRefreshTime >= REFRESH_INTERVAL) {
            lastRefreshTime = currentMillis;
            updateNowPlayingScreen();
        }
    }
}
