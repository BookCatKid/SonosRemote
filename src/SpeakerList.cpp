#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "SpeakerList.h"
#include "UIGlobals.h"

extern Adafruit_ST7789 tft;

void SpeakerList::drawHeader(const char* fullText) {
    tft.fillRect(0, 0, 240, 30, 0x7BEF);
    tft.setFont();
    tft.setTextSize(1);
    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(centerX(fullText, 1), 12);
    tft.print(fullText);
}

void SpeakerList::drawHeader() {
    drawHeader("Speakers");
}

void SpeakerList::setSelectedIndex(int index) {
    selectedIndex = index;
}

void SpeakerList::drawDeviceRow(int index, const SonosDevice& device, int y, bool isSelected) {
    if (isSelected) {
        tft.fillRect(0, y, 240, 28, 0x4208);
        tft.drawRect(2, y + 2, 236, 24, ST77XX_WHITE);
    } else {
        tft.fillRect(0, y, 240, 28, ST77XX_BLACK);
        tft.drawFastHLine(10, y + 27, 220, 0x4208);
    }

    tft.setFont();
    tft.setTextSize(1);

    if (isSelected) {
        tft.setTextColor(ST77XX_YELLOW);
    } else {
        tft.setTextColor(ST77XX_WHITE);
    }

    String label = String(index + 1) + ". " + device.name;
    tft.setCursor(16, y + 4);
    tft.print(label);

    tft.setTextColor(0x7BEF);
    tft.setCursor(16, y + 16);
    tft.print(device.ip);
}

void SpeakerList::drawScanButton(int y, bool isSelected) {
    if (isSelected) {
        tft.fillRect(0, y, 240, 28, 0x07E0); // Greenish for scan
        tft.drawRect(2, y + 2, 236, 24, ST77XX_WHITE);
        tft.setTextColor(ST77XX_BLACK);
    } else {
        tft.fillRect(0, y, 240, 28, ST77XX_BLACK);
        tft.drawRect(2, y + 2, 236, 24, 0x07E0);
        tft.setTextColor(0x07E0);
    }

    tft.setFont();
    tft.setTextSize(1);
    const char* label = "SCAN FOR NEW SPEAKERS";
    tft.setCursor(centerX(label, 1), y + 10);
    tft.print(label);
}

void SpeakerList::drawDevices(const std::vector<SonosDevice>& devices) {
    int y = 36;
    int maxVisible = (280 - 36) / 28;
    int i = 0;

    for (; i < (int)devices.size() && i < maxVisible - 1; i++) {
        bool isSelected = (i == selectedIndex);
        drawDeviceRow(i, devices[i], y, isSelected);
        y += 28;
    }

    // Always draw scan button as the last item
    bool scanSelected = (selectedIndex == (int)devices.size() || selectedIndex == i);
    // If list is long, we might need better scroll logic, but for now:
    drawScanButton(y, selectedIndex == (int)devices.size());
}

void SpeakerList::draw(Sonos& sonos) {
    tft.fillScreen(ST77XX_BLACK);
    drawHeader();
    auto devices = sonos.getDiscoveredDevices();
    drawDevices(devices);
}

void SpeakerList::draw(const std::vector<SonosDevice>& devices) {
    tft.fillScreen(ST77XX_BLACK);
    drawHeader();
    drawDevices(devices);
}

void SpeakerList::updateHeader(const char* statusText) {
    String fullText = String("Speakers (") + statusText + ")";
    drawHeader(fullText.c_str());
}

void SpeakerList::clearDeviceList() {
    tft.fillRect(0, 36, 240, 280 - 36, ST77XX_BLACK);
}

void SpeakerList::refreshDevices(const std::vector<SonosDevice>& devices) {
    clearDeviceList();
    drawDevices(devices);
}
