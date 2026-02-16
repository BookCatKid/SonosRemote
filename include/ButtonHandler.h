#pragma once
#include <Arduino.h>
#include <Adafruit_MCP23X17.h>

class ButtonHandler {
public:
    ButtonHandler(Adafruit_MCP23X17& mcp, int upPin, int downPin, int clickPin, int volUpPin, int volDownPin);
    void begin();
    void update();
    bool upPressed();
    bool downPressed();
    bool clickPressed();
    bool volUpPressed();
    bool volDownPressed();
    bool upLongPressed();
    bool downLongPressed();
    bool clickLongPressed();

private:
    Adafruit_MCP23X17& _mcp;
    int upPin, downPin, clickPin, volUpPin, volDownPin;
    bool upState, downState, clickState, volUpState, volDownState;
    bool upPrev, downPrev, clickPrev, volUpPrev, volDownPrev;
    bool upJustPressed, downJustPressed, clickJustPressed, volUpJustPressed, volDownJustPressed;
    bool upJustLongPressed, downJustLongPressed, clickJustLongPressed;
    bool upLongPressHandled, downLongPressHandled, clickLongPressHandled;
    unsigned long upPressStartTime, downPressStartTime, clickPressStartTime;
    unsigned long lastDebounceTime;
    static const unsigned long DEBOUNCE_DELAY = 50;
    static const unsigned long LONG_PRESS_TIME = 600;
};