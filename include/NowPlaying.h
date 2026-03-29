#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

class NowPlaying {
public:
    void drawStatic();
    void update();
    void drawStatusBar(const char* statusText);
    void drawAlbumArt();
    void drawAlbumArt(const char* url);
    void drawTrackInfo(const char* song, const char* artist, const char* album);
    void drawProgressBar(int position, int duration);
    void drawVolume(int volume);
    void drawSpeakerInfo(const char* name);

private:
    static void albumArtDownloadTask(void* param);
    void startAlbumArtDownloadIfNeeded();
    void drawLoadingAlbumArt();

    String _requestedAlbumArtUrl;
    String _loadingAlbumArtUrl;
    String _displayedAlbumArtUrl;
    TaskHandle_t _albumArtTaskHandle = nullptr;
    volatile bool _albumArtDownloadInProgress = false;
    volatile bool _albumArtReady = false;
    volatile bool _albumArtFailed = false;
    uint8_t* _albumArtBuffer = nullptr;
    size_t _albumArtLength = 0;
};
