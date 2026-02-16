#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

class NowPlaying {
public:
    void drawStatic();
    void drawStatusBar(const char* statusText);
    void drawAlbumArt();
    void drawAlbumArt(const char* url);
    void drawTrackInfo(const char* song, const char* artist);
    void drawProgressBar(int progress);
    void drawVolume(int volume);
    void drawSpeakerInfo(const char* name);
};
