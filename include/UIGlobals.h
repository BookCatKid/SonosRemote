#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>

extern int16_t centerX(const char* text, uint8_t textSize);
extern int16_t printCenteredWrapped(Adafruit_GFX& gfx, const char* text, int16_t y, uint16_t w, uint8_t textSize);

