#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "NowPlaying.h"

#define TFT_CS  D3
#define TFT_DC  D2
#define TFT_RST D1
#define TFT_BL  D4

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);
NowPlaying nowPlaying;

int progress = 0;
int volume = 50;

void setup() {
    Serial.begin(115200);

    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    tft.init(240, 280);
    tft.setRotation(0);

    nowPlaying.drawStatic();
    nowPlaying.drawTrackInfo("Blinding Lights", "The Weeknd");
    nowPlaying.drawSpeakerInfo("Living Room");
    nowPlaying.drawProgressBar(progress);
    nowPlaying.drawVolume(volume);
}

void loop() {
    progress++;
    if (progress > 100) progress = 0;

    nowPlaying.drawProgressBar(progress);
    delay(100);
}
