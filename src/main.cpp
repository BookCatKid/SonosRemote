#include <Arduino.h>
#include <WiFi.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Sonos.h>
#include "NowPlaying.h"
#include "SpeakerList.h"
#include "secrets.h"

#define TFT_CS  D3
#define TFT_DC  D2
#define TFT_RST D1
#define TFT_BL  D4

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
Sonos sonos;
NowPlaying nowPlaying;
SpeakerList speakerList;

int progress = 0;
int volume = 50;

void setup() {
    Serial.begin(115200);

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    tft.init(240, 280);
    tft.setRotation(0);

    tft.fillScreen(ST77XX_BLACK);
    tft.setFont();
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_CYAN);
    tft.setCursor(60, 140);
    tft.print("Connecting Wi-Fi...");

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
        return;
    }

    tft.fillScreen(ST77XX_BLACK);
    tft.setTextColor(ST77XX_CYAN);
    tft.setCursor(48, 140);
    tft.print("Discovering Sonos...");

    SonosConfig config;
    config.discoveryTimeoutMs = 10000;
    sonos.setConfig(config);
    sonos.begin();
    SonosResult result = sonos.discoverDevices();
    if (result != SonosResult::SUCCESS) {
        tft.fillScreen(ST77XX_BLACK);
        tft.setTextColor(ST77XX_RED);
        tft.setCursor(54, 140);
        tft.print("Discovery failed!");
        return;
    }

    speakerList.draw(sonos);
}

void loop() {
}
