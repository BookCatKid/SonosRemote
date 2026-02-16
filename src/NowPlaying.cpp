#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <TJpg_Decoder.h>
#include <HTTPClient.h>
#include "NowPlaying.h"
#include "FreeMono9pt7b.h"
#include "Petme8x8.h"
#include "UIGlobals.h"

extern Adafruit_ST7789 tft;

extern const unsigned char image_song_cover_bits[];
extern const unsigned char image_volume_normal_bits[];

// Callback function to draw decoded JPEG blocks
static bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    if (y >= tft.height()) return false;
    tft.drawRGBBitmap(x, y, bitmap, w, h);
    return true;
}

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

void NowPlaying::drawAlbumArt(const char* url) {
    if (url == nullptr || strlen(url) == 0) {
        drawAlbumArt(); // Fallback to default
        return;
    }

    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        int len = http.getSize();
        if (len > 0) {
            uint8_t* buffer = (uint8_t*)malloc(len);
            if (buffer) {
                WiFiClient* stream = http.getStreamPtr();
                stream->readBytes(buffer, len);
                
                TJpgDec.setCallback(tft_output);
                
                uint16_t w = 0, h = 0;
                TJpgDec.getJpgSize(&w, &h, buffer, len);
                
                // Target max dimension of ~140 pixels
                uint8_t scale = 1;
                if (w > 140 * 4 || h > 140 * 4) scale = 8;
                else if (w > 140 * 2 || h > 140 * 2) scale = 4;
                else if (w > 140 || h > 140) scale = 2;
                
                TJpgDec.setJpgScale(scale);
                
                int drawW = w / scale;
                int drawH = h / scale;
                int x = (240 - drawW) / 2;
                int y = 62 + (120 - drawH) / 2; // Center within the 120px vertical slot
                
                // Clear the middle area before drawing (from below speaker info to above progress bar)
                tft.fillRect(0, 58, 240, 132, ST77XX_BLACK);
                
                TJpgDec.drawJpg(x, y, buffer, len);
                
                free(buffer);
            }
        }
    } else {
        drawAlbumArt(); // Fallback
    }
    http.end();
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
