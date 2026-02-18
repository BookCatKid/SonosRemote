#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <TJpg_Decoder.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "NowPlaying.h"
#include "UIGlobals.h"
#include "AppLogger.h"
#include "Images.h"

extern Adafruit_ST7789 tft;

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
    int centerX = 120, centerY = 105;
    tft.fillRect(0, 58, 240, 94, ST77XX_BLACK);
    tft.drawCircle(centerX, centerY, 40, 0x4208);
    tft.drawCircle(centerX, centerY, 38, 0x4208);
    tft.drawCircle(centerX, centerY, 12, 0xAD55);
    tft.fillCircle(centerX, centerY, 3, ST77XX_WHITE);
    tft.setTextColor(0x4208);
    tft.setCursor(centerX - 20, centerY + 45);
    tft.print("NO ART");
}

void NowPlaying::drawAlbumArt(const char* url) {
    if (url == nullptr || strlen(url) == 0) {
        drawAlbumArt();
        return;
    }

    LOG_DEBUG("image", "Fetching album art: " + String(url));
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    int httpCode = -1;
    String urlStr = String(url);
    if (urlStr.startsWith("https://")) {
        WiFiClientSecure sc; sc.setInsecure();
        if (http.begin(sc, urlStr)) httpCode = http.GET();
    } else {
        if (http.begin(urlStr)) httpCode = http.GET();
    }

    if (httpCode == HTTP_CODE_OK) {
        int len = http.getSize();
        if (len > 0) {
            size_t freeHeap = ESP.getFreeHeap();
            if (freeHeap < (size_t)len + 8192) {
                LOG_WARN("image", "Not enough memory for image. freeHeap=" + String(freeHeap) + " size=" + String(len));
                drawAlbumArt();
                http.end();
                return;
            }

            uint8_t* buffer = (uint8_t*)malloc(len);
            if (buffer) {
                if (http.getStream().readBytes(buffer, len) > 0) {
                    TJpgDec.setCallback(tft_output);
                    uint16_t w, h;
                    if (TJpgDec.getJpgSize(&w, &h, buffer, len) == JDR_OK) {
                        uint8_t scale = 1;
                        if (w > 320 || h > 320) scale = 8;
                        else if (w > 160 || h > 160) scale = 4;
                        else if (w > 80 || h > 80) scale = 2;

                        TJpgDec.setJpgScale(scale);
                        int drawW = w / scale;
                        int drawH = h / scale;
                        int x = (240 - drawW) / 2;
                        int y = 58 + (94 - drawH) / 2;

                        tft.fillRect(0, 58, 240, 94, ST77XX_BLACK);
                        TJpgDec.drawJpg(x, y, buffer, len);
                    } else {
                        LOG_WARN("image", "Failed to decode JPG metadata");
                        drawAlbumArt();
                    }
                }
                free(buffer);
            } else {
                LOG_WARN("image", "Failed to allocate image buffer");
            }
        }
    } else {
        LOG_WARN("image", "Album art fetch failed. HTTP code=" + String(httpCode));
        drawAlbumArt();
    }
    http.end();
}

void NowPlaying::drawTrackInfo(const char* song, const char* artist, const char* album) {
    tft.setTextColor(ST77XX_WHITE);
    tft.fillRect(0, 180, 240, 70, ST77XX_BLACK);
    tft.setFont();
    uint8_t tS = 2;
    int16_t sL = strlen(song);

    int16_t currentY = 183;
    if (sL > (240 / 12)) {
        tS = 1; tft.setTextSize(1);
        if (sL > (240 / 6)) {
            currentY = printCenteredWrapped(tft, song, currentY, 240, 1);
        } else {
            tft.setCursor(centerX(song, 1), currentY); tft.print(song);
            currentY += 10;
        }
    } else {
        tft.setTextSize(2); tft.setCursor(centerX(song, 2), currentY); tft.print(song);
        currentY += 20;
    }

    tft.setTextSize(1); tft.setTextColor(0xAD55);
    currentY += 10;
    currentY = printCenteredWrapped(tft, artist, currentY, 240, 1);

    if (album && strlen(album) > 0) {
        currentY += 10;
        printCenteredWrapped(tft, album, currentY, 240, 1);
    }
}


static String formatTime(int seconds) {
    if (seconds < 0) seconds = 0;
    int m = seconds / 60, s = seconds % 60;
    char buf[10]; sprintf(buf, "%02d:%02d", m, s);
    return String(buf);
}

void NowPlaying::drawProgressBar(int position, int duration) {
    int progress = (duration > 0) ? (position * 100) / duration : 0;
    if (progress > 100) progress = 100;
    int bW = 130, x = 10, y = 162, h = 10;
    tft.fillRect(x, y, bW, h, 0x4208);
    tft.fillRect(x, y, map(progress, 0, 100, 0, bW), h, 0x5FF3);
    String tT = formatTime(position) + " / " + formatTime(duration);
    tft.setTextColor(ST77XX_WHITE);
    tft.fillRect(x + bW + 5, y - 2, 240 - (x + bW + 5), h + 4, ST77XX_BLACK);
    tft.setFont(); tft.setTextSize(1); tft.setCursor(x + bW + 10, y + 1); tft.print(tT);
}

void NowPlaying::drawVolume(int volume) {
    int w = map(volume, 0, 100, 0, 151);
    tft.fillRect(56, 255, 151, 12, ST77XX_BLACK);
    tft.fillRect(56, 255, 151, 12, 0x4208);
    tft.fillRect(56, 255, w, 12, ST77XX_BLUE);
    tft.drawBitmap(33, 253, image_volume_normal_bits, 18, 16, ST77XX_WHITE);
}

void NowPlaying::drawSpeakerInfo(const char* name) {
    tft.fillRect(0, 33, 240, 24, 0x7BEF);
    tft.setFont(); tft.setTextSize(2);
    tft.setCursor(centerX(name, 2), 37); tft.print(name);
}
