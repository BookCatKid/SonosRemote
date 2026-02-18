#pragma once
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <Sonos.h>
#include <vector>

class SpeakerList {
public:
    void draw(Sonos& sonos);
    void draw(const std::vector<SonosDevice>& devices);
    void drawHeader();
    void drawHeader(const char* fullText);
    void updateHeader(const char* statusText);
    void refreshDevices(const std::vector<SonosDevice>& devices);
    void updateSelection(const std::vector<SonosDevice>& devices);
    void setSelectedIndex(int index);

private:
    int selectedIndex = 0;
    int previousSelectedIndex = -1;
    void drawDeviceRow(int index, const SonosDevice& device, int y, bool isSelected);
    void drawScanButton(int y, bool isSelected);
    void drawDevices(const std::vector<SonosDevice>& devices);
    void clearDeviceList();
};
