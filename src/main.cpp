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

void updateStatusBar(const char* status) {
    nowPlaying.drawStatusBar(status);
}

void updateStatusBar(const String& status) {
    nowPlaying.drawStatusBar(status.c_str());
}

void connectWiFi() {
    updateStatusBar("WiFi: Connecting...");

    WiFi.persistent(true);
    WiFi.mode(WIFI_STA);
#if USE_STATIC_IP
    WiFi.config(STATIC_IP, GATEWAY, SUBNET);
#endif
    WiFi.begin(ssid, password);
    WiFi.setAutoReconnect(true);

    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
        delay(100);
    }

    if (WiFi.status() != WL_CONNECTED) {
        tft.fillScreen(ST77XX_BLACK);
        tft.setTextColor(ST77XX_RED);
        tft.setCursor(72, 140);
        tft.print("Wi-Fi Failed!");
        while (true) { delay(1000); }
    }

    updateStatusBar("WiFi: Connected");
}

void discoverDevices(bool background = false) {
    if (!background) {
        updateStatusBar("Sonos: Discovering...");
    }

    SonosConfig config;
    config.discoveryTimeoutMs = 10000;
    sonos.setConfig(config);
    sonos.begin();

    SonosResult result = sonos.discoverDevices();

    if (result == SonosResult::SUCCESS) {
        auto devices = sonos.getDiscoveredDevices();
        if (devices.size() > 0) {
            deviceCache.saveDevices(devices);
            updateStatusBar("Sonos: Found " + String((int)devices.size()) + " speakers");
        } else {
            updateStatusBar("Sonos: No speakers");
        }
    } else {
        updateStatusBar("Sonos: Discovery failed");
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

    connectWiFi();

    bool loadedFromCache = false;
    auto cachedDevices = deviceCache.loadDevices();

    if (cachedDevices.size() > 0) {
        updateStatusBar("Sonos: Loading cached...");
        std::vector<SonosDevice> devices;
        for (const auto& cached : cachedDevices) {
            SonosDevice dev;
            dev.name = cached.name;
            dev.ip = cached.ip;
            devices.push_back(dev);
        }

        speakerList.draw(devices);
        updateStatusBar("Sonos: " + String((int)devices.size()) + " speakers (cached)");
        loadedFromCache = true;
        delay(1000);
    }

    discoverDevices(loadedFromCache);
    speakerList.draw(sonos);
}

void loop() {
}
