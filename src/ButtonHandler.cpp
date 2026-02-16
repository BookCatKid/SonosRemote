#include "ButtonHandler.h"

ButtonHandler::ButtonHandler(Adafruit_MCP23X17& mcp, int upPin, int downPin, int clickPin, int volUpPin, int volDownPin)
    : _mcp(mcp), upPin(upPin), downPin(downPin), clickPin(clickPin), volUpPin(volUpPin), volDownPin(volDownPin),
      upState(false), downState(false), clickState(false), volUpState(false), volDownState(false),
      upPrev(false), downPrev(false), clickPrev(false), volUpPrev(false), volDownPrev(false),
      upJustPressed(false), downJustPressed(false), clickJustPressed(false), volUpJustPressed(false), volDownJustPressed(false),
      upJustLongPressed(false), downJustLongPressed(false), clickJustLongPressed(false),
      upLongPressHandled(false), downLongPressHandled(false), clickLongPressHandled(false),
      upPressStartTime(0), downPressStartTime(0), clickPressStartTime(0),
      lastDebounceTime(0) {}

void ButtonHandler::begin() {
    _mcp.pinMode(upPin, INPUT_PULLUP);
    _mcp.pinMode(downPin, INPUT_PULLUP);
    _mcp.pinMode(clickPin, INPUT_PULLUP);
    _mcp.pinMode(volUpPin, INPUT_PULLUP);
    _mcp.pinMode(volDownPin, INPUT_PULLUP);
}

void ButtonHandler::update() {
    bool upReading = (_mcp.digitalRead(upPin) == LOW);
    bool downReading = (_mcp.digitalRead(downPin) == LOW);
    bool clickReading = (_mcp.digitalRead(clickPin) == LOW);
    bool volUpReading = (_mcp.digitalRead(volUpPin) == LOW);
    bool volDownReading = (_mcp.digitalRead(volDownPin) == LOW);

    if (upReading != upPrev || downReading != downPrev || clickReading != clickPrev || 
        volUpReading != volUpPrev || volDownReading != volDownPrev) {
        lastDebounceTime = millis();
    }

    upPrev = upReading;
    downPrev = downReading;
    clickPrev = clickReading;
    volUpPrev = volUpReading;
    volDownPrev = volDownReading;

    if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
        unsigned long now = millis();

        // UP Button logic
        if (upReading && !upState) {
            upPressStartTime = now;
            upLongPressHandled = false;
        } else if (upReading && !upLongPressHandled && (now - upPressStartTime > LONG_PRESS_TIME)) {
            upJustLongPressed = true;
            upLongPressHandled = true;
        } else if (!upReading && upState && !upLongPressHandled) {
            upJustPressed = true;
        }
        upState = upReading;

        // DOWN Button logic
        if (downReading && !downState) {
            downPressStartTime = now;
            downLongPressHandled = false;
        } else if (downReading && !downLongPressHandled && (now - downPressStartTime > LONG_PRESS_TIME)) {
            downJustLongPressed = true;
            downLongPressHandled = true;
        } else if (!downReading && downState && !downLongPressHandled) {
            downJustPressed = true;
        }
        downState = downReading;

        // CLICK Button logic
        if (clickReading && !clickState) {
            clickPressStartTime = now;
            clickLongPressHandled = false;
        } else if (clickReading && !clickLongPressHandled && (now - clickPressStartTime > LONG_PRESS_TIME)) {
            clickJustLongPressed = true;
            clickLongPressHandled = true;
        } else if (!clickReading && clickState && !clickLongPressHandled) {
            clickJustPressed = true;
        }
        clickState = clickReading;

        // VOLUME buttons (simple press)
        if (volUpReading && !volUpState) {
            volUpJustPressed = true;
        }
        volUpState = volUpReading;

        if (volDownReading && !volDownState) {
            volDownJustPressed = true;
        }
        volDownState = volDownReading;
    } else {
        upJustPressed = downJustPressed = clickJustPressed = false;
        volUpJustPressed = volDownJustPressed = false;
        upJustLongPressed = downJustLongPressed = clickJustLongPressed = false;
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

bool ButtonHandler::volUpPressed() {
    if (volUpJustPressed) {
        volUpJustPressed = false;
        return true;
    }
    return false;
}

bool ButtonHandler::volDownPressed() {
    if (volDownJustPressed) {
        volDownJustPressed = false;
        return true;
    }
    return false;
}

bool ButtonHandler::upLongPressed() {
    if (upJustLongPressed) {
        upJustLongPressed = false;
        return true;
    }
    return false;
}

bool ButtonHandler::downLongPressed() {
    if (downJustLongPressed) {
        downJustLongPressed = false;
        return true;
    }
    return false;
}

bool ButtonHandler::clickLongPressed() {
    if (clickJustLongPressed) {
        clickJustLongPressed = false;
        return true;
    }
    return false;
}
