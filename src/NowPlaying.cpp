#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "NowPlaying.h"
#include "FreeMono9pt7b.h"
#include "Petme8x8.h"
#include "UIGlobals.h"

extern Adafruit_ST7789 tft;

extern const unsigned char image_song_cover_bits[];
extern const unsigned char image_volume_normal_bits[];

void NowPlaying::drawStatic() {
    tft.fillScreen(ST77XX_BLACK);

    drawStatusBar("Ready");
    drawAlbumArt();
}

void NowPlaying::drawStatusBar(const char* statusText) {
    tft.fillRect(0, 1, 240, 30, 0x7BEF);
    tft.setTextColor(ST77XX_WHITE);
    tft.setFont();
    tft.setTextSize(1);
    int16_t x = centerX(statusText, 1);
    tft.setCursor(x, 12);
    tft.print(statusText);
}

void NowPlaying::drawAlbumArt() {
    tft.drawBitmap(60, 62, image_song_cover_bits, 120, 120, ST77XX_WHITE);
}

void NowPlaying::drawTrackInfo(const char* song, const char* artist) {
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextWrap(false);

    tft.setFont();
    tft.setTextSize(1);
    tft.fillRect(0, 210, 240, 40, ST77XX_BLACK);
    tft.setCursor(centerX(song, 1), 215);
    tft.print(song);

    tft.setFont();
    tft.setTextSize(1);
    tft.setCursor(centerX(artist, 1), 228);
    tft.print(artist);
}

void NowPlaying::drawProgressBar(int progress) {
    int width = map(progress, 0, 100, 0, 193);

    tft.fillRect(23, 191, 193, 17, ST77XX_WHITE);
    tft.fillRect(23, 191, width, 17, 0x5FF3);
}

void NowPlaying::drawVolume(int volume) {
    int width = map(volume, 0, 100, 0, 151);

    tft.fillRect(56, 250, 151, 7, ST77XX_WHITE);
    tft.fillRect(56, 250, width, 7, ST77XX_BLUE);

    tft.drawBitmap(33, 246, image_volume_normal_bits, 18, 16, ST77XX_WHITE);
}

void NowPlaying::drawSpeakerInfo(const char* name) {
    tft.fillRect(0, 33, 240, 24, 0x7BEF);
    tft.setFont();
    tft.setTextSize(2);
    tft.setCursor(centerX(name, 2), 37);
    tft.print(name);
}
