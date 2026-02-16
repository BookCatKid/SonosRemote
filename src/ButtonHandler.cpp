#include "ButtonHandler.h"

ButtonHandler::ButtonHandler(Adafruit_MCP23X17& mcp, int upPin, int downPin, int clickPin)
    : _mcp(mcp), upPin(upPin), downPin(downPin), clickPin(clickPin),
      upState(false), downState(false), clickState(false),
      upPrev(false), downPrev(false), clickPrev(false),
      upJustPressed(false), downJustPressed(false), clickJustPressed(false),
      lastDebounceTime(0) {}

void ButtonHandler::begin() {
    _mcp.pinMode(upPin, INPUT_PULLUP);
    _mcp.pinMode(downPin, INPUT_PULLUP);
    _mcp.pinMode(clickPin, INPUT_PULLUP);
}

void ButtonHandler::update() {
    // Read current button states (LOW when pressed due to pull-up)
    bool upReading = (_mcp.digitalRead(upPin) == LOW);
    bool downReading = (_mcp.digitalRead(downPin) == LOW);
    bool clickReading = (_mcp.digitalRead(clickPin) == LOW);

    // Check if any button state changed
    if (upReading != upPrev || downReading != downPrev || clickReading != clickPrev) {
        lastDebounceTime = millis();
    }

    // Update previous raw states
    upPrev = upReading;
    downPrev = downReading;
    clickPrev = clickReading;

    // If debounce period has passed, check for state changes
    if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
        // Detect rising edge (button just pressed)
        upJustPressed = upReading && !upState;
        downJustPressed = downReading && !downState;
        clickJustPressed = clickReading && !clickState;

        // Update stable pressed states
        upState = upReading;
        downState = downReading;
        clickState = clickReading;
    } else {
        // Clear edge flags during debounce
        upJustPressed = false;
        downJustPressed = false;
        clickJustPressed = false;
    }
}

bool ButtonHandler::upPressed() {
    if (upJustPressed) {
        upJustPressed = false;
        return true;
    }
    return false;
}

bool ButtonHandler::downPressed() {
    if (downJustPressed) {
        downJustPressed = false;
        return true;
    }
    return false;
}

bool ButtonHandler::clickPressed() {
    if (clickJustPressed) {
        clickJustPressed = false;
        return true;
    }
    return false;
}