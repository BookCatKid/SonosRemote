#include "UIGlobals.h"

int16_t centerX(const char* text, uint8_t textSize) {
    int16_t charWidth = 6 * textSize;
    int32_t textWidth = (int32_t)strlen(text) * charWidth;
    int16_t x = (240 - textWidth) / 2;
    if (x < 0) return 0; // Don't start off-screen
    return x;
}
