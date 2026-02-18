#include "UIGlobals.h"
#include "Images.h"

const unsigned char* getWifiIcon() {
    if (wifiState == WIFI_CONNECTED) return image_wifi_full_bits;
    if (wifiState == WIFI_DISCONNECTED) return image_wifi_not_connected_bits;

    uint32_t m = millis() % 1000;
    if (m < 250) return image_wifi_25_bits;
    if (m < 500) return image_wifi_50_bits;
    if (m < 750) return image_wifi_75_bits;
    return image_wifi_full_bits;
}

int16_t centerX(const char* text, uint8_t textSize) {
    int16_t charWidth = 6 * textSize;
    int32_t textWidth = (int32_t)strlen(text) * charWidth;
    int16_t x = (240 - textWidth) / 2;
    if (x < 0) return 0; // Don't start off-screen
    return x;
}

int16_t printCenteredWrapped(Adafruit_GFX& gfx, const char* text, int16_t y, uint16_t w, uint8_t textSize) {
    String str = String(text);
    int len = str.length();
    if (len == 0) return y;

    gfx.setTextSize(textSize);
    gfx.setTextWrap(false); // We handle wrapping manually

    int16_t charWidth = 6 * textSize;
    int16_t lineHeight = 8 * textSize;
    int16_t maxChars = w / charWidth;

    String currentLine = "";
    int start = 0;

    while (start < len) {
        int end = str.indexOf(' ', start);
        if (end == -1) end = len;

        String word = str.substring(start, end);

        if (currentLine.length() + word.length() + (currentLine.length() > 0 ? 1 : 0) <= maxChars) {
            if (currentLine.length() > 0) currentLine += " ";
            currentLine += word;
        } else {
            if (currentLine.length() > 0) {
                int32_t lineWidth = (int32_t)currentLine.length() * charWidth;
                int16_t x = (w - lineWidth) / 2;
                if (x < 0) x = 0;
                gfx.setCursor(x, y);
                gfx.print(currentLine);
                y += lineHeight;
                currentLine = word;
            } else {
                // Word is huge, split it
                while (word.length() > maxChars) {
                    String part = word.substring(0, maxChars);
                    gfx.setCursor(0, y); // Full width, so x=0
                    gfx.print(part);
                    y += lineHeight;
                    word = word.substring(maxChars);
                }
                currentLine = word;
            }
        }
        start = end + 1;
    }

    if (currentLine.length() > 0) {
        int32_t lineWidth = (int32_t)currentLine.length() * charWidth;
        int16_t x = (w - lineWidth) / 2;
        if (x < 0) x = 0;
        gfx.setCursor(x, y);
        gfx.print(currentLine);
        y += lineHeight;
    }

    return y;
}
