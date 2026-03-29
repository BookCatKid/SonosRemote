#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <TJpg_Decoder.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "NowPlaying.h"
#include "UIGlobals.h"
#include "AppLogger.h"
#include "Images.h"

extern Adafruit_ST7789 tft;

namespace {
constexpr int MAX_ALBUM_ART_BYTES = 180 * 1024;
constexpr uint16_t ALBUM_ART_HTTP_TIMEOUT_MS = 5000;
constexpr uint16_t ALBUM_ART_IDLE_TIMEOUT_MS = 3000;
constexpr size_t ALBUM_ART_INITIAL_UNKNOWN_SIZE = 8192;
portMUX_TYPE g_albumArtMux = portMUX_INITIALIZER_UNLOCKED;

void formatTimeText(int seconds, char* out, size_t outSize) {
    if (seconds < 0) seconds = 0;
    int h = seconds / 3600;
    int m = (seconds % 3600) / 60;
    int s = seconds % 60;
    if (h > 0) {
        snprintf(out, outSize, "%d:%02d:%02d", h, m, s);
    } else {
        snprintf(out, outSize, "%02d:%02d", m, s);
    }
}
}

static bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    if (y >= tft.height()) return false;
    tft.drawRGBBitmap(x, y, bitmap, w, h);
    return true;
}

void NowPlaying::drawStatic() {
    // New screen session: clear URL cache so re-entering now playing shows loading state again.
    _requestedAlbumArtUrl = "";
    _loadingAlbumArtUrl = "";
    _displayedAlbumArtUrl = "";

    portENTER_CRITICAL(&g_albumArtMux);
    if (_albumArtBuffer != nullptr) {
        free(_albumArtBuffer);
        _albumArtBuffer = nullptr;
    }
    _albumArtLength = 0;
    _albumArtReady = false;
    _albumArtFailed = false;
    portEXIT_CRITICAL(&g_albumArtMux);

    tft.fillScreen(ST77XX_BLACK);
    drawStatusBar("Ready");
    drawAlbumArt();
}

void NowPlaying::update() {
    bool ready = false;
    bool failed = false;
    uint8_t* buffer = nullptr;
    size_t len = 0;

    portENTER_CRITICAL(&g_albumArtMux);
    ready = _albumArtReady;
    failed = _albumArtFailed;
    if (ready) {
        buffer = _albumArtBuffer;
        len = _albumArtLength;
        _albumArtBuffer = nullptr;
        _albumArtLength = 0;
        _albumArtReady = false;
        _albumArtFailed = false;
    } else if (failed) {
        _albumArtFailed = false;
    }
    portEXIT_CRITICAL(&g_albumArtMux);

    if (ready && buffer != nullptr && len > 0) {
        if (_loadingAlbumArtUrl != _requestedAlbumArtUrl) {
            free(buffer);
            startAlbumArtDownloadIfNeeded();
            return;
        }

        TJpgDec.setCallback(tft_output);
        uint16_t w = 0;
        uint16_t h = 0;
        if (TJpgDec.getJpgSize(&w, &h, buffer, len) == JDR_OK) {
            uint8_t scale = 1;
            if (w > 320 || h > 320) scale = 8;
            else if (w > 160 || h > 160) scale = 4;
            else if (w > 80 || h > 80) scale = 2;

            TJpgDec.setJpgScale(scale);
            int drawW = w / scale;
            int drawH = h / scale;
            int x = (240 - drawW) / 2;
            int y = 58 + (102 - drawH) / 2;

            tft.fillRect(0, 58, 240, 102, ST77XX_BLACK);
            TJpgDec.drawJpg(x, y, buffer, len);
            _displayedAlbumArtUrl = _loadingAlbumArtUrl;
        } else {
            LOG_WARN("image", "Failed to decode downloaded JPG");
            drawAlbumArt();
            _displayedAlbumArtUrl = _loadingAlbumArtUrl;
        }
        free(buffer);
    } else if (failed) {
        LOG_WARN("image", "Album art download failed");
        drawAlbumArt();
        _displayedAlbumArtUrl = _loadingAlbumArtUrl;
    }

    startAlbumArtDownloadIfNeeded();
}

void NowPlaying::drawStatusBar(const char* statusText) {
    tft.fillRect(0, 1, 240, 30, 0x7BEF);
    tft.setTextColor(ST77XX_WHITE);
    tft.setFont();
    tft.setTextSize(1);
    int16_t x = centerX(statusText, 1);
    tft.setCursor(x, 12);
    tft.print(statusText);

    tft.drawBitmap(205, 7, getWifiIcon(), 18, 16, ST77XX_WHITE);
}

void NowPlaying::drawAlbumArt() {
    int centerX = 120, centerY = 109;
    tft.fillRect(0, 58, 240, 102, ST77XX_BLACK);
    tft.drawCircle(centerX, centerY, 40, 0x4208);
    tft.drawCircle(centerX, centerY, 38, 0x4208);
    tft.drawCircle(centerX, centerY, 12, 0xAD55);
    tft.fillCircle(centerX, centerY, 3, ST77XX_WHITE);
    tft.setTextColor(0x4208);
    tft.setCursor(centerX - 20, centerY + 40);
    tft.print("NO ART");
}

void NowPlaying::drawLoadingAlbumArt() {
    tft.fillRect(0, 58, 240, 102, ST77XX_BLACK);
    tft.drawRect(20, 70, 200, 78, 0x4208);
    tft.drawRect(22, 72, 196, 74, 0x4208);
    tft.setTextColor(0x7BEF);
    tft.setFont();
    tft.setTextSize(1);
    tft.setCursor(centerX("LOADING ART...", 1), 100);
    tft.print("LOADING ART...");
    tft.setTextColor(0x4208);
    tft.setCursor(centerX("Please wait", 1), 116);
    tft.print("Please wait");
}

void NowPlaying::startAlbumArtDownloadIfNeeded() {
    if (_albumArtDownloadInProgress) return;
    if (_requestedAlbumArtUrl.length() == 0) return;
    if (_requestedAlbumArtUrl == _displayedAlbumArtUrl) return;

    _loadingAlbumArtUrl = _requestedAlbumArtUrl;
    _albumArtDownloadInProgress = true;

    BaseType_t created = xTaskCreatePinnedToCore(
        &NowPlaying::albumArtDownloadTask,
        "art_dl",
        8192,
        this,
        1,
        &_albumArtTaskHandle,
        0
    );

    if (created != pdPASS) {
        _albumArtDownloadInProgress = false;
        _albumArtTaskHandle = nullptr;
        LOG_WARN("image", "Failed to create album art task");
    }
}

void NowPlaying::albumArtDownloadTask(void* param) {
    NowPlaying* self = static_cast<NowPlaying*>(param);
    if (self == nullptr) {
        vTaskDelete(nullptr);
        return;
    }

    String url = self->_loadingAlbumArtUrl;
    int httpCode = -1;
    uint8_t* buffer = nullptr;
    size_t len = 0;
    bool success = false;

    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(ALBUM_ART_HTTP_TIMEOUT_MS);

    if (strncmp(url.c_str(), "https://", 8) == 0) {
        WiFiClientSecure secureClient;
        secureClient.setInsecure();
        if (http.begin(secureClient, url)) {
            httpCode = http.GET();
        }
    } else {
        if (http.begin(url)) {
            httpCode = http.GET();
        }
    }

    if (httpCode == HTTP_CODE_OK) {
        size_t capacity = ALBUM_ART_INITIAL_UNKNOWN_SIZE;
        size_t freeHeap = ESP.getFreeHeap();
        if (freeHeap < capacity + 8192) {
            LOG_WARN("image", "Insufficient heap for art. freeHeap=" + String(freeHeap));
        } else {
            buffer = static_cast<uint8_t*>(malloc(capacity));
            if (buffer == nullptr) {
                LOG_WARN("image", "Failed to allocate album art buffer");
            } else {
                Stream& stream = http.getStream();
                unsigned long lastDataMs = millis();

                while (true) {
                    int availableBytes = stream.available();
                    while (availableBytes > 0) {
                        if (len >= MAX_ALBUM_ART_BYTES) {
                            LOG_WARN("image", "Album art exceeds max supported bytes");
                            break;
                        }

                        if (len >= capacity) {
                            size_t nextCapacity = capacity * 2;
                            if (nextCapacity > MAX_ALBUM_ART_BYTES) {
                                nextCapacity = MAX_ALBUM_ART_BYTES;
                            }
                            if (nextCapacity <= capacity) {
                                break;
                            }

                            uint8_t* grown = static_cast<uint8_t*>(realloc(buffer, nextCapacity));
                            if (grown == nullptr) {
                                LOG_WARN("image", "Failed to grow album art buffer");
                                break;
                            }
                            buffer = grown;
                            capacity = nextCapacity;
                        }

                        size_t room = capacity - len;
                        size_t toRead = room;
                        if (toRead > static_cast<size_t>(availableBytes)) {
                            toRead = static_cast<size_t>(availableBytes);
                        }

                        int read = stream.readBytes(buffer + len, toRead);
                        if (read <= 0) {
                            break;
                        }

                        len += static_cast<size_t>(read);
                        availableBytes = stream.available();
                        lastDataMs = millis();
                    }

                    bool connected = http.connected();
                    if ((!connected && stream.available() == 0) || len >= MAX_ALBUM_ART_BYTES) {
                        break;
                    }

                    if (millis() - lastDataMs > ALBUM_ART_IDLE_TIMEOUT_MS) {
                        LOG_WARN("image", "Album art stream idle timeout");
                        break;
                    }

                    delay(1);
                }

                if (len > 0) {
                    success = true;
                    uint8_t* shrink = static_cast<uint8_t*>(realloc(buffer, len));
                    if (shrink != nullptr) {
                        buffer = shrink;
                    }
                } else {
                    free(buffer);
                    buffer = nullptr;
                }
            }
        }
    } else {
        LOG_WARN("image", "Album art fetch failed. HTTP code=" + String(httpCode));
    }

    http.end();

    portENTER_CRITICAL(&g_albumArtMux);
    self->_albumArtBuffer = buffer;
    self->_albumArtLength = len;
    self->_albumArtReady = success;
    self->_albumArtFailed = !success;
    self->_albumArtDownloadInProgress = false;
    self->_albumArtTaskHandle = nullptr;
    portEXIT_CRITICAL(&g_albumArtMux);

    vTaskDelete(nullptr);
}

void NowPlaying::drawAlbumArt(const char* url) {
    if (url == nullptr || strlen(url) == 0) {
        _requestedAlbumArtUrl = "";
        _displayedAlbumArtUrl = "";
        drawAlbumArt();
        return;
    }

    if (strstr(url, ".png") != nullptr || strstr(url, ".PNG") != nullptr) {
        _requestedAlbumArtUrl = "";
        _displayedAlbumArtUrl = "";
        drawAlbumArt();
        tft.fillRect(0, 140, 240, 20, ST77XX_BLACK);
        tft.setTextColor(0x4208);
        tft.setCursor(centerX("UNSUPPORTED", 1), 145);
        tft.print("UNSUPPORTED");
        return;
    }

    String requested = String(url);
    if (requested == _displayedAlbumArtUrl || requested == _requestedAlbumArtUrl) {
        return;
    }

    _requestedAlbumArtUrl = requested;
    drawLoadingAlbumArt();
    LOG_DEBUG("image", "Queued album art fetch: " + requested);
    startAlbumArtDownloadIfNeeded();
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


void NowPlaying::drawProgressBar(int position, int duration) {
    int progress = (duration > 0) ? (position * 100) / duration : 0;
    if (progress > 100) progress = 100;
    int bW = 130, x = 10, y = 162, h = 10;
    tft.fillRect(x, y, bW, h, 0x4208);
    tft.fillRect(x, y, map(progress, 0, 100, 0, bW), h, 0x5FF3);
    char posText[12];
    char durText[12];
    char timeline[28];
    formatTimeText(position, posText, sizeof(posText));
    formatTimeText(duration, durText, sizeof(durText));
    snprintf(timeline, sizeof(timeline), "%s / %s", posText, durText);
    tft.setTextColor(ST77XX_WHITE);
    tft.fillRect(x + bW + 5, y - 2, 240 - (x + bW + 5), h + 4, ST77XX_BLACK);
    tft.setFont(); tft.setTextSize(1); tft.setCursor(x + bW + 10, y + 1); tft.print(timeline);
}

void NowPlaying::drawVolume(int volume) {
    int w = map(volume, 0, 100, 0, 151);
    tft.fillRect(56, 255, 151, 12, ST77XX_BLACK);
    tft.fillRect(56, 255, 151, 12, 0x4208);
    tft.fillRect(56, 255, w, 12, ST77XX_BLUE);

    const unsigned char* icon = image_volume_normal_bits;
    if (volume == 0) icon = image_volume_muted_bits;
    else if (volume < 33) icon = image_volume_low_bits;
    else if (volume > 66) icon = image_volume_loud_bits;

    tft.fillRect(33, 253, 18, 16, ST77XX_BLACK);
    tft.drawBitmap(33, 253, icon, 18, 16, ST77XX_WHITE);
}

void NowPlaying::drawSpeakerInfo(const char* name) {
    tft.fillRect(0, 33, 240, 24, 0x7BEF);
    tft.setFont(); tft.setTextSize(2);
    tft.setCursor(centerX(name, 2), 37); tft.print(name);
}
