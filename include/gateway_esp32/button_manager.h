#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <Arduino.h>

// Button gesture detection
enum ButtonGesture {
  GESTURE_NONE,
  GESTURE_SINGLE_CLICK,
  GESTURE_DOUBLE_CLICK,
  GESTURE_LONG_PRESS_START,
  GESTURE_LONG_PRESS_STOP
};

class ButtonManager {
 public:
  ButtonManager(int pin, bool activeHigh = true);

  // Initialize button
  void begin();

  // Call this in loop() - returns detected gesture
  ButtonGesture update();

  // Check if button is currently pressed
  bool isPressed() const { return stablePressed; }

  // Configuration
  void setDebounceTime(unsigned long ms) { debounceTime = ms; }
  void setDoubleClickTime(unsigned long ms) { doubleClickTime = ms; }
  void setLongPressTime(unsigned long ms) { longPressTime = ms; }
  // Separate debounce time to ignore very short releases (useful during
  // long-press when mechanical contacts may briefly open).
  void setReleaseDebounceTime(unsigned long ms) { releaseDebounceTime = ms; }

 private:
  // Pin configuration
  int buttonPin;
  bool activeHigh;

  // Timing configuration
  unsigned long debounceTime = 30;         // ms
  unsigned long doubleClickTime = 350;     // ms
  unsigned long longPressTime = 800;       // ms
  unsigned long releaseDebounceTime = 80;  // ms - ignore very short releases

  // Debounce state
  int lastRaw = HIGH;
  unsigned long lastBounceTime = 0;
  bool stablePressed = false;
  bool prevStablePressed = false;

  // Gesture state
  unsigned long pressStart = 0;
  bool longPressEmitted = false;
  bool waitingForSecondClick = false;
  unsigned long lastReleaseTime = 0;
};

#endif  // BUTTON_MANAGER_H
