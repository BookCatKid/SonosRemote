#include <Arduino.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Sonos.h>
#include <vector>
#include "NowPlaying.h"
#include "SpeakerList.h"
#include "secrets.h"
#include "DeviceCache.h"
#include "ButtonHandler.h"

#define TFT_CS  D3
#define TFT_DC  D2
#define TFT_RST D1
#define TFT_BL  D4

// Button pins (using internal pull-ups, connect button between pin and GND)
#define BTN_UP    D5
#define BTN_DOWN  D6
#define BTN_CLICK D7

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
Sonos sonos;
NowPlaying nowPlaying;
SpeakerList speakerList;
DeviceCache deviceCache;

int progress = 0;
int volume = 50;
bool needsDiscoveryRefresh = true;

// Button handler
ButtonHandler buttons(BTN_UP, BTN_DOWN, BTN_CLICK);

// Navigation state (only for speaker list)
int selectedIndex = 0;
std::vector<SonosDevice> currentDevices;
IPAddress selectedDeviceIP;

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
unsigned long wifiConnectStartTime = 0;
const unsigned long WIFI_TIMEOUT = 30000; // 30 seconds timeout

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
    wifiConnectStartTime = millis();
    Serial.println("Starting WiFi connection...");
    Serial.print("SSID: ");
    Serial.println(ssid);
}

void checkWiFiConnection() {
    if (wifiState == WIFI_CONNECTING) {
        if (WiFi.status() == WL_CONNECTED) {
            wifiState = WIFI_CONNECTED;
            Serial.println("WiFi connected!");
            Serial.print("IP: ");
            Serial.println(WiFi.localIP());
        } else if (millis() - wifiConnectStartTime > WIFI_TIMEOUT) {
            wifiState = WIFI_DISCONNECTED;
            Serial.println("WiFi connection timeout!");
            // Retry connection
            WiFi.disconnect();
            delay(100);
            startWiFiConnection();
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
            currentDevices = devices;
            // Ensure selectedIndex is valid
            if (selectedIndex >= (int)currentDevices.size()) {
                selectedIndex = currentDevices.size() - 1;
            }
            String status = String((int)devices.size()) + " speakers";
            speakerList.updateHeader(status.c_str());
            speakerList.setSelectedIndex(selectedIndex);
            speakerList.refreshDevices(currentDevices);
        } else {
            speakerList.updateHeader("No speakers");
            currentDevices.clear();
            selectedIndex = 0;
        }
    } else {
        speakerList.updateHeader("Discovery failed");
    }
}

void handleSpeakerListNavigation() {
    if (currentDevices.size() == 0) return;

    bool selectionChanged = false;

    // Handle UP button
    if (buttons.upPressed()) {
        if (selectedIndex > 0) {
            selectedIndex--;
            selectionChanged = true;
        }
    }

    // Handle DOWN button
    if (buttons.downPressed()) {
        if (selectedIndex < (int)currentDevices.size() - 1) {
            selectedIndex++;
            selectionChanged = true;
        }
    }

    // Handle CLICK button
    if (buttons.clickPressed()) {
        selectedDeviceIP.fromString(currentDevices[selectedIndex].ip.c_str());
        currentScreen = SCREEN_NOW_PLAYING;
        nowPlaying.drawStatic();
        nowPlaying.drawSpeakerInfo(currentDevices[selectedIndex].name.c_str());
        return;
    }

    // Update display if selection changed
    if (selectionChanged) {
        speakerList.setSelectedIndex(selectedIndex);
        speakerList.refreshDevices(currentDevices);
    }
}

void setup() {
    Serial.begin(115200);
    deviceCache.begin();

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    tft.init(240, 280);
    tft.setRotation(2);

    // Initialize buttons
    buttons.begin();

    tft.fillScreen(ST77XX_BLACK);

    // Load and show cached devices immediately without waiting for WiFi
    auto cachedDevices = deviceCache.loadDevices();
    currentDevices.clear();

    if (cachedDevices.size() > 0) {
        for (const auto& cached : cachedDevices) {
            SonosDevice dev;
            dev.name = cached.name;
            dev.ip = cached.ip;
            currentDevices.push_back(dev);
        }
        speakerList.setSelectedIndex(0);
        speakerList.draw(currentDevices);
    } else {
        // No cache, show empty list with status
        speakerList.draw(currentDevices);
    }

    // Start WiFi connection in the background
    startWiFiConnection();
}

void loop() {
    // Check WiFi connection status
    checkWiFiConnection();

    // Update button states
    buttons.update();

    // Handle button input (only on speaker list screen)
    if (currentScreen == SCREEN_SPEAKER_LIST) {
        handleSpeakerListNavigation();
    }

    // Update header only when WiFi state changes
    if (wifiState != previousWifiState) {
        previousWifiState = wifiState;
        if (wifiState == WIFI_CONNECTING) {
            speakerList.updateHeader("WiFi: Connecting...");
        } else if (wifiState == WIFI_CONNECTED) {
            speakerList.updateHeader("WiFi: Connected");
        } else if (wifiState == WIFI_DISCONNECTED) {
            speakerList.updateHeader("WiFi: Failed");
        }
    }

    // Do initial discovery once WiFi is connected - DISABLED, using cache only
    // if (wifiState == WIFI_CONNECTED && needsInitialDiscovery) {
    //     needsInitialDiscovery = false;
    //     speakerList.updateHeader("Discovering...");
    //     discoverDevices();
    // }
    
    // Periodic refresh when on speaker list - DISABLED
    // if (currentScreen == SCREEN_SPEAKER_LIST && wifiState == WIFI_CONNECTED) {
    //     unsigned long currentTime = millis();
    //     if (currentTime - lastRefreshTime >= REFRESH_INTERVAL) {
    //         lastRefreshTime = currentTime;
    //         discoverDevices();
    //     }
    // }
}
