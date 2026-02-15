#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include "SpeakerList.h"

extern Adafruit_ST7789 tft;

static int16_t centerX(const char* text, uint8_t textSize) {
    int16_t charWidth = 6 * textSize;
    int16_t textWidth = strlen(text) * charWidth;
    return (240 - textWidth) / 2;
}

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

void SpeakerList::drawDevices(const std::vector<SonosDevice>& devices) {
    if (devices.size() == 0) {
        tft.setFont();
        tft.setTextSize(1);
        tft.setTextColor(0x7BEF);
        tft.setCursor(centerX("No speakers found", 1), 140);
        tft.print("No speakers found");
        return;
    }

    int y = 36;
    int maxVisible = (280 - 36) / 28;

    for (int i = 0; i < (int)devices.size() && i < maxVisible; i++) {
        bool isSelected = (i == selectedIndex);
        drawDeviceRow(i, devices[i], y, isSelected);
        y += 28;
    }

    if ((int)devices.size() > maxVisible) {
        String more = String((int)devices.size() - maxVisible) + " more...";
        tft.setTextColor(0x7BEF);
        tft.setCursor(centerX(more.c_str(), 1), 270);
        tft.print(more);
    }
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
