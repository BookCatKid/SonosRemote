#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Sonos.h>
#include <vector>

class SpeakerList {
public:
    void draw(Sonos& sonos);
    void draw(const std::vector<SonosDevice>& devices);
    void updateHeader(const char* statusText);
    void refreshDevices(const std::vector<SonosDevice>& devices);

private:
    void drawHeader();
    void drawHeader(const char* fullText);
    void drawDeviceRow(int index, const SonosDevice& device, int y);
    void drawDevices(const std::vector<SonosDevice>& devices);
    void clearDeviceList();
};
