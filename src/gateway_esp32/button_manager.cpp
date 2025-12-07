#include "../../include/gateway_esp32/button_manager.h"

ButtonManager::ButtonManager(int pin, bool activeHigh)
    : buttonPin(pin), activeHigh(activeHigh) {}

void ButtonManager::begin() {
  if (activeHigh) {
    pinMode(buttonPin, INPUT);  // External pull-down
  } else {
    pinMode(buttonPin, INPUT_PULLUP);  // Internal pull-up
  }
  
  Serial.printf("[Button] Initialized on pin %d (Active %s)\n",
                buttonPin, activeHigh ? "HIGH" : "LOW");
}

ButtonGesture ButtonManager::update() {
  ButtonGesture detectedGesture = GESTURE_NONE;

  // Read raw pin and normalize to 'rawPressed' boolean
  int raw = digitalRead(buttonPin);
  bool rawPressed = (activeHigh ? (raw == HIGH) : (raw == LOW));

  // Debounce
  if (raw != lastRaw) {
    lastBounceTime = millis();
  }

  if ((millis() - lastBounceTime) > debounceTime) {
    stablePressed = rawPressed;
  }

  lastRaw = raw;

  // Detect press event
  if (stablePressed && !prevStablePressed) {
    // Button pressed
    pressStart = millis();
    longPressEmitted = false;
  }

  // Check for long press while button is held
  if (stablePressed && !longPressEmitted) {
    if (millis() - pressStart >= longPressTime) {
      // Long press detected
      detectedGesture = GESTURE_LONG_PRESS_START;
      longPressEmitted = true;
      waitingForSecondClick = false;  // Cancel any pending single-click
    }
  }

  // Detect release event
  if (!stablePressed && prevStablePressed) {
    unsigned long pressDuration = millis() - pressStart;

    if (longPressEmitted) {
      // Long press released
      detectedGesture = GESTURE_LONG_PRESS_STOP;
      longPressEmitted = false;
    } else {
      // Short press - candidate for single/double click
      if (waitingForSecondClick) {
        // Second click within window
        if (millis() - lastReleaseTime <= doubleClickTime) {
          detectedGesture = GESTURE_DOUBLE_CLICK;
          waitingForSecondClick = false;
        } else {
          // First wait expired, this is a new first click
          waitingForSecondClick = true;
          lastReleaseTime = millis();
        }
      } else {
        // First click - start waiting for possible double click
        waitingForSecondClick = true;
        lastReleaseTime = millis();
      }
    }
  }

  // Check if single-click timeout expired
  if (waitingForSecondClick &&
      (millis() - lastReleaseTime > doubleClickTime)) {
    detectedGesture = GESTURE_SINGLE_CLICK;
    waitingForSecondClick = false;
  }

  prevStablePressed = stablePressed;

  return detectedGesture;
}
