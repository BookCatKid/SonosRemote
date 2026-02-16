#pragma once
#include <Arduino.h>
#include <Adafruit_MCP23X17.h>

class ButtonHandler {
public:
    ButtonHandler(Adafruit_MCP23X17& mcp, int upPin, int downPin, int clickPin);
    void begin();
    void update();
    bool upPressed();
    bool downPressed();
    bool clickPressed();

private:
    Adafruit_MCP23X17& _mcp;
    int upPin, downPin, clickPin;
    bool upState, downState, clickState;
    bool upPrev, downPrev, clickPrev;
    bool upJustPressed, downJustPressed, clickJustPressed;
    unsigned long lastDebounceTime;
    static const unsigned long DEBOUNCE_DELAY = 50;
};