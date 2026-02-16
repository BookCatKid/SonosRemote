#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <TJpg_Decoder.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "NowPlaying.h"
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
    // Placeholder: A stylized vinyl record
    int centerX = 120;
    int centerY = 122;
    tft.fillRect(0, 58, 240, 132, ST77XX_BLACK);
    tft.drawCircle(centerX, centerY, 50, 0x4208); // Outer ring
    tft.drawCircle(centerX, centerY, 48, 0x4208);
    tft.drawCircle(centerX, centerY, 15, 0xAD55); // Label
    tft.fillCircle(centerX, centerY, 3, ST77XX_WHITE); // Center hole
    
    tft.setTextColor(0x4208);
    tft.setCursor(centerX - 20, centerY + 55);
    tft.print("NO ART");
}

void NowPlaying::drawAlbumArt(const char* url) {
    if (url == nullptr || strlen(url) == 0) {
        drawAlbumArt();
        return;
    }

    Serial.print("[Sonos] Fetching art: "); Serial.println(url);

    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    
    int httpCode = -1;
    String urlStr = String(url);

    if (urlStr.startsWith("https://")) {
        WiFiClientSecure secureClient;
        secureClient.setInsecure();
        if (http.begin(secureClient, urlStr)) {
            httpCode = http.GET();
        }
    } else {
        if (http.begin(urlStr)) {
            httpCode = http.GET();
        }
    }

    if (httpCode == -1) {
        Serial.print("[Sonos] Connection failed (error: ");
        Serial.print(http.errorToString(httpCode));
        Serial.println(")");
        
        if (urlStr.indexOf("192.168.") == -1) {
            Serial.println("[Sonos] External URL failed, checking DNS...");
            IPAddress dns(8, 8, 8, 8);
            WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(), dns);
            delay(500);
            
            if (urlStr.startsWith("https://")) {
                WiFiClientSecure secureClient;
                secureClient.setInsecure();
                if (http.begin(secureClient, urlStr)) httpCode = http.GET();
            } else {
                if (http.begin(urlStr)) httpCode = http.GET();
            }
        }
    }

    Serial.print("[Sonos] HTTP code: "); Serial.println(httpCode);

    if (httpCode == HTTP_CODE_OK) {
        int len = http.getSize();
        if (len > 0) {
            size_t freeHeap = ESP.getFreeHeap();
            if (freeHeap < (size_t)len + 8192) {
                Serial.println("[Sonos] Not enough memory for image");
                drawAlbumArt();
                http.end();
                return;
            }

            uint8_t* buffer = (uint8_t*)malloc(len);
            if (buffer) {
                int read = http.getStream().readBytes(buffer, len);
                if (read > 0) {
                    TJpgDec.setCallback(tft_output);
                    uint16_t w = 0, h = 0;
                    if (TJpgDec.getJpgSize(&w, &h, buffer, read) == JDR_OK) {
                        // Target max dimension of 80 pixels for a compact look
                        uint8_t scale = 1;
                        if (w > 80 * 4 || h > 80 * 4) scale = 8;
                        else if (w > 80 * 2 || h > 80 * 2) scale = 4;
                        else if (w > 80 || h > 80) scale = 2;

                        TJpgDec.setJpgScale(scale);
                        int drawW = w / scale;
                        int drawH = h / scale;
                        int x = (240 - drawW) / 2;
                        int y = 62 + (120 - drawH) / 2;

                        tft.fillRect(0, 58, 240, 132, ST77XX_BLACK);
                        TJpgDec.drawJpg(x, y, buffer, read);
                    } else {
                        drawAlbumArt();
                    }
                }
                free(buffer);
            }
        }
    } else {
        drawAlbumArt();
    }
    http.end();
}

void NowPlaying::drawTrackInfo(const char* song, const char* artist, const char* album) {
    tft.setTextColor(ST77XX_WHITE);
    tft.setTextWrap(false);

    tft.fillRect(0, 205, 240, 45, ST77XX_BLACK);
    
    tft.setFont();
    tft.setTextSize(1);
    tft.setCursor(centerX(song, 1), 208);
    tft.print(song);

    tft.setTextColor(0xAD55); // Grayish
    tft.setCursor(centerX(artist, 1), 220);
    tft.print(artist);

    if (album && strlen(album) > 0) {
        tft.setCursor(centerX(album, 1), 232);
        tft.print(album);
    }
}

void NowPlaying::drawProgressBar(int progress) {
    int width = map(progress, 0, 100, 0, 193);

    tft.fillRect(23, 188, 193, 12, ST77XX_WHITE);
    tft.fillRect(23, 188, width, 12, 0x5FF3);
}

void NowPlaying::drawVolume(int volume) {
    int width = map(volume, 0, 100, 0, 151);

    tft.fillRect(56, 255, 151, 10, ST77XX_BLACK);
    tft.fillRect(56, 255, 151, 10, 0x4208);
    tft.fillRect(56, 255, width, 10, ST77XX_BLUE);

    tft.drawBitmap(33, 252, image_volume_normal_bits, 18, 16, ST77XX_WHITE);
}

void NowPlaying::drawSpeakerInfo(const char* name) {
    tft.fillRect(0, 33, 240, 24, 0x7BEF);
    tft.setFont();
    tft.setTextSize(2);
    tft.setCursor(centerX(name, 2), 37);
    tft.print(name);
}
