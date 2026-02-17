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
#include "AppLogger.h"
#include "SonosEventManager.h"

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
SonosEventManager eventManager(8080);
ButtonHandler buttons(mcp, BTN_UP, BTN_DOWN, BTN_CLICK, BTN_VUP, BTN_VDOWN);

int selectedIndex = 0;
IPAddress selectedDeviceIP;

// UI tracking
String lastAlbumArtUrl = "";
String lastTitle = "";
String lastArtist = "";
String lastAlbum = "";
String lastPlaybackState = "";
int lastPositionSeconds = -1;
int lastDurationSeconds = -1;
int lastVolume = -1;
bool forcePositionSync = false;
unsigned long lastPositionSyncMs = 0;
const unsigned long POSITION_SYNC_INTERVAL_MS = 10000;
bool needsInitialNowPlayingFetch = false;
unsigned long lastInitialFetchAttemptMs = 0;
const unsigned long INITIAL_FETCH_RETRY_INTERVAL_MS = 3000;

enum ScreenState {
    SCREEN_SPEAKER_LIST,
    SCREEN_NOW_PLAYING
};

ScreenState currentScreen = SCREEN_SPEAKER_LIST;

enum WiFiState {
    WIFI_DISCONNECTED,
    WIFI_CONNECTING,
    WIFI_CONNECTED
};

WiFiState wifiState = WIFI_DISCONNECTED;
WiFiState previousWifiState = WIFI_DISCONNECTED;
unsigned long wifiConnectStartTime = 0;
const unsigned long WIFI_TIMEOUT = 30000; // 30 seconds timeout

template <size_t N>
static String formatChannelList(const char* const (&channels)[N], bool shouldDisplay = true) {
    if (!shouldDisplay || N == 0) return "(none)";
    String out;
    for (size_t i = 0; i < N; i++) {
        if (i > 0) out += ", ";
        out += channels[i];
    }
    return out;
}

void startWiFiConnection() {
    LOG_INFO("wifi", "Starting WiFi connection");
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
            LOG_INFO("wifi", "WiFi connected: " + WiFi.localIP().toString());
            sonos.begin();
            eventManager.begin();
        } else if (millis() - wifiConnectStartTime > WIFI_TIMEOUT) {
            wifiState = WIFI_DISCONNECTED;
            LOG_WARN("wifi", "WiFi connect timeout, retrying");
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
            forcePositionSync = true;
            lastPositionSyncMs = 0;
            needsInitialNowPlayingFetch = true;
            lastInitialFetchAttemptMs = 0;

            // Subscribe to events
            eventManager.subscribe(selectedDeviceIP.toString(), "AVTransport");
            eventManager.subscribe(selectedDeviceIP.toString(), "RenderingControl");

            // Full redraw reset
            lastAlbumArtUrl = lastTitle = lastArtist = lastAlbum = lastPlaybackState = "";
            lastPositionSeconds = lastDurationSeconds = lastVolume = -1;

            nowPlaying.drawStatic();
            nowPlaying.drawSpeakerInfo(devices[selectedIndex].name.c_str());
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
        return;
    }

    if (buttons.clickLongPressed()) {
        eventManager.unsubscribe(selectedDeviceIP.toString(), "AVTransport");
        eventManager.unsubscribe(selectedDeviceIP.toString(), "RenderingControl");
        forcePositionSync = false;
        needsInitialNowPlayingFetch = false;
        currentScreen = SCREEN_SPEAKER_LIST;
        speakerList.draw(discoveryManager.getDevices());
        return;
    }

    if (buttons.upPressed()) {
        sonosController.next(selectedDeviceIP.toString());
    }
    if (buttons.downPressed()) {
        sonosController.previous(selectedDeviceIP.toString());
    }

    if (buttons.volUpPressed()) {
        sonosController.volumeUp(selectedDeviceIP.toString());
    }
    if (buttons.volDownPressed()) {
        sonosController.volumeDown(selectedDeviceIP.toString());
    }
}

void updateNowPlayingScreen() {
    if (wifiState != WIFI_CONNECTED) return;
    sonosController.tick();
    unsigned long nowMs = millis();

    if (needsInitialNowPlayingFetch &&
        (lastInitialFetchAttemptMs == 0 || nowMs - lastInitialFetchAttemptMs >= INITIAL_FETCH_RETRY_INTERVAL_MS)) {
        lastInitialFetchAttemptMs = nowMs;
        if (sonosController.update(selectedDeviceIP.toString())) {
            needsInitialNowPlayingFetch = false;
            forcePositionSync = false;
            lastPositionSyncMs = nowMs;
            LOG_DEBUG("control", "Initial now-playing fetch succeeded");
        } else {
            LOG_WARN("control", "Initial now-playing fetch failed; retry scheduled");
        }
    }

    const auto& preSyncData = sonosController.getTrackData();
    bool isPlaying = preSyncData.playbackState == "PLAYING" || preSyncData.playbackState == "TRANSITIONING";
    bool periodicSyncDue = isPlaying && (nowMs - lastPositionSyncMs >= POSITION_SYNC_INTERVAL_MS);

    if (forcePositionSync || periodicSyncDue) {
        if (sonosController.refreshPosition(selectedDeviceIP.toString(), true)) {
            lastPositionSyncMs = nowMs;
        }
        forcePositionSync = false;
    }

    const auto& data = sonosController.getTrackData();

    if (data.title != lastTitle || data.artist != lastArtist || data.album != lastAlbum) {
        lastTitle = data.title;
        lastArtist = data.artist;
        lastAlbum = data.album;
        nowPlaying.drawTrackInfo(data.title.c_str(), data.artist.c_str(), data.album.c_str());
    }

    if (data.position != lastPositionSeconds || data.duration != lastDurationSeconds) {
        lastPositionSeconds = data.position;
        lastDurationSeconds = data.duration;
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
}

void setup() {
    WiFi.mode(WIFI_STA); // Initialize stack early
    Serial.begin(115200);
    AppLogger::begin(Serial, LogLevel::DEBUG);
    AppLogger::setEnabled(true);

    // `useAllowList=false` means allow all channels (except blocked ones below).
    // `useAllowList=true` means only channels in this list are shown.
    const bool useAllowList = true;
    const char* allowedLogChannels[] = {
        "core", "wifi", "discovery", "cache", "xml", "soap", "control", "playback", "image", "ui", "events"
    };
    AppLogger::clearAllowedChannels();
    if (useAllowList) {
        for (const char* channel : allowedLogChannels) {
            AppLogger::allowChannel(channel);
        }
    }

    // These channels are always hidden, even if included in allowed list.
    const char* blockedLogChannels[] = {
        "soap"
    };
    AppLogger::clearBlockedChannels();
    for (const char* channel : blockedLogChannels) {
        AppLogger::blockChannel(channel);
    }

    LOG_INFO("core", "Sonos Remote starting");
    LOG_INFO("core", "Log level set to DEBUG");
    LOG_INFO("core", "Allow-list mode: " + String(useAllowList ? "enabled" : "disabled"));
    LOG_INFO("core", "Allowed channels: " + formatChannelList(allowedLogChannels, useAllowList));
    LOG_INFO("core", "Blocked channels: " + formatChannelList(blockedLogChannels));
    deviceCache.begin();

    if (!mcp.begin_I2C(0x20)) {
        LOG_ERROR("core", "MCP23017 not found");
    }

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

    eventManager.setEventCallback([](const String& ip, const String& service, const String& data) {
        (void)service;
        if (currentScreen != SCREEN_NOW_PLAYING || ip != selectedDeviceIP.toString()) {
            return;
        }

        LOG_DEBUG("core", "Event received from " + ip);
        sonosController.parseEvent(data);

        // Ensure album art URL is absolute if relative
        auto& track = const_cast<TrackData&>(sonosController.getTrackData());
        if (track.albumArtUrl.length() > 0 && track.albumArtUrl.startsWith("/")) {
            track.albumArtUrl = "http://" + ip + ":1400" + track.albumArtUrl;
        }

        if (data.indexOf("CurrentTrackURI") != -1 ||
            data.indexOf("CurrentTrackMetaData") != -1 ||
            data.indexOf("CurrentTrack val=") != -1) {
            forcePositionSync = true;
            LOG_DEBUG("control", "Track-related event received; scheduling position sync");
        }

    });

    speakerList.setSelectedIndex(0);
    speakerList.draw(discoveryManager.getDevices());

    startWiFiConnection();
}

void loop() {
    checkWiFiConnection();
    buttons.update();
    eventManager.update();

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
        updateNowPlayingScreen();
    }
}
