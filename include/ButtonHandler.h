#pragma once
#include <Arduino.h>

class ButtonHandler {
public:
    ButtonHandler(int upPin, int downPin, int clickPin);
    void begin();
    void update();
    bool upPressed();
    bool downPressed();
    bool clickPressed();

private:
    int upPin, downPin, clickPin;
    bool upState, downState, clickState;
    bool upPrev, downPrev, clickPrev;
    bool upJustPressed, downJustPressed, clickJustPressed;
    unsigned long lastDebounceTime;
    static const unsigned long DEBOUNCE_DELAY = 50;
};