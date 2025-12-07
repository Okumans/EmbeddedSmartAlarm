#include "../../include/gateway_esp32/button_manager.h"

ButtonManager::ButtonManager(int pin, bool activeHigh)
    : buttonPin(pin), activeHigh(activeHigh) {}

void ButtonManager::begin() {
  if (activeHigh) {
    pinMode(buttonPin, INPUT);  // External pull-down
  } else {
    pinMode(buttonPin, INPUT_PULLUP);  // Internal pull-up
  }

  Serial.printf("[Button] Initialized on pin %d (Active %s)\n", buttonPin,
                activeHigh ? "HIGH" : "LOW");
}

ButtonGesture ButtonManager::update() {
  ButtonGesture detectedGesture = GESTURE_NONE;

  // Read raw pin and normalize to 'rawPressed' boolean
  int raw = digitalRead(buttonPin);
  bool rawPressed = (activeHigh ? (raw == HIGH) : (raw == LOW));

  // Debounce: require raw to be stable for `debounceTime` before accepting
  // a change. This avoids flicker. Additionally, when a long press has
  // already been emitted, ignore very short release candidates (e.g. brief
  // mechanical openings) shorter than `releaseDebounceTime` so the long
  // press isn't accidentally treated as a release.
  if (raw != lastRaw) {
    lastBounceTime = millis();
  }

  bool candidatePressed = stablePressed;  // default to current
  if ((millis() - lastBounceTime) > debounceTime) {
    candidatePressed = rawPressed;
  }

  if (candidatePressed != stablePressed) {
    // If we're seeing a release candidate while a long-press was emitted,
    // ignore it unless it has been stable for `releaseDebounceTime`.
    if (!candidatePressed && longPressEmitted) {
      if ((millis() - lastBounceTime) > releaseDebounceTime) {
        stablePressed = candidatePressed;
      } else {
        // treat as still pressed (ignore brief release)
      }
    } else {
      stablePressed = candidatePressed;
    }
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
  if (waitingForSecondClick && (millis() - lastReleaseTime > doubleClickTime)) {
    detectedGesture = GESTURE_SINGLE_CLICK;
    waitingForSecondClick = false;
  }

  prevStablePressed = stablePressed;

  return detectedGesture;
}
