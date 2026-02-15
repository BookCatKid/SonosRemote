#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Sonos.h>

class SpeakerList {
public:
    void draw(Sonos& sonos);

private:
    void drawHeader();
    void drawDeviceRow(int index, const SonosDevice& device, int y);
};
