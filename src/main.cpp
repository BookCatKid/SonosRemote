#include <Arduino.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Sonos.h>
#include "NowPlaying.h"
#include "SpeakerList.h"
#include "secrets.h"
#include "DeviceCache.h"

#define TFT_CS  D3
#define TFT_DC  D2
#define TFT_RST D1
#define TFT_BL  D4

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
Sonos sonos;
NowPlaying nowPlaying;
SpeakerList speakerList;
DeviceCache deviceCache;

int progress = 0;
int volume = 50;
bool needsDiscoveryRefresh = true;

enum ScreenState {
    SCREEN_SPEAKER_LIST,
    SCREEN_NOW_PLAYING
};

ScreenState currentScreen = SCREEN_SPEAKER_LIST;
unsigned long lastRefreshTime = 0;
const unsigned long REFRESH_INTERVAL = 10000; // Refresh every 10 seconds

enum WiFiState {
    WIFI_DISCONNECTED,
    WIFI_CONNECTING,
    WIFI_CONNECTED
};

WiFiState wifiState = WIFI_DISCONNECTED;
WiFiState previousWifiState = WIFI_DISCONNECTED;
bool needsInitialDiscovery = true;

void updateStatusBar(const char* status) {
    nowPlaying.drawStatusBar(status);
}

void updateStatusBar(const String& status) {
    nowPlaying.drawStatusBar(status.c_str());
}

void startWiFiConnection() {
    WiFi.persistent(true);
    WiFi.mode(WIFI_STA);
#if USE_STATIC_IP
    WiFi.config(STATIC_IP, GATEWAY, SUBNET);
#endif
    WiFi.begin(ssid, password);
    WiFi.setAutoReconnect(true);
    wifiState = WIFI_CONNECTING;
}

void checkWiFiConnection() {
    if (wifiState == WIFI_CONNECTING) {
        if (WiFi.status() == WL_CONNECTED) {
            wifiState = WIFI_CONNECTED;
        }
    }
}

void discoverDevices() {
    SonosConfig config;
    config.discoveryTimeoutMs = 5000;
    sonos.setConfig(config);
    sonos.begin();

    SonosResult result = sonos.discoverDevices();

    if (result == SonosResult::SUCCESS) {
        auto devices = sonos.getDiscoveredDevices();
        if (devices.size() > 0) {
            deviceCache.saveDevices(devices);
            String status = String((int)devices.size()) + " speakers";
            speakerList.updateHeader(status.c_str());
            speakerList.refreshDevices(devices);
        } else {
            speakerList.updateHeader("No speakers");
        }
    } else {
        speakerList.updateHeader("Discovery failed");
    }
}

void setup() {
    Serial.begin(115200);
    deviceCache.begin();

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    tft.init(240, 280);
    tft.setRotation(0);

    tft.fillScreen(ST77XX_BLACK);

    // Load and show cached devices immediately without waiting for WiFi
    auto cachedDevices = deviceCache.loadDevices();
    std::vector<SonosDevice> devices;
    
    if (cachedDevices.size() > 0) {
        for (const auto& cached : cachedDevices) {
            SonosDevice dev;
            dev.name = cached.name;
            dev.ip = cached.ip;
            devices.push_back(dev);
        }
        speakerList.draw(devices);
    } else {
        // No cache, show empty list with status
        speakerList.draw(devices);
    }

    // Start WiFi connection in the background
    startWiFiConnection();
}

void loop() {
    // Check WiFi connection status
    checkWiFiConnection();
    
    // Update header only when WiFi state changes
    if (wifiState != previousWifiState) {
        previousWifiState = wifiState;
        if (wifiState == WIFI_CONNECTING) {
            speakerList.updateHeader("WiFi: Connecting...");
        }
    }
    
    // Do initial discovery once WiFi is connected
    if (wifiState == WIFI_CONNECTED && needsInitialDiscovery) {
        needsInitialDiscovery = false;
        speakerList.updateHeader("Discovering...");
        discoverDevices();
    }
    
    // Periodic refresh when on speaker list
    if (currentScreen == SCREEN_SPEAKER_LIST && wifiState == WIFI_CONNECTED) {
        unsigned long currentTime = millis();
        if (currentTime - lastRefreshTime >= REFRESH_INTERVAL) {
            lastRefreshTime = currentTime;
            discoverDevices();
        }
    }
}
