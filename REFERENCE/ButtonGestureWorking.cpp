// Button gesture demo — robust debounce + single/double/long detection
// Set BUTTON_ACTIVE_HIGH to true if your button pulls the pin HIGH when pressed (external pull-down).
// Set it to false if your button pulls the pin LOW when pressed (using INPUT_PULLUP).
// Example: external pull-down -> BUTTON_ACTIVE_HIGH = true
//          internal pull-up  -> BUTTON_ACTIVE_HIGH = false

#define BUTTON_PIN 4
const bool BUTTON_ACTIVE_HIGH = true; // <--- set according to your wiring

// Timing (tune to taste)
const unsigned long DEBOUNCE_TIME   = 30;   // ms
const unsigned long DOUBLE_CLICK_MS = 350;  // max gap between clicks for double click
const unsigned long LONG_PRESS_MS   = 800;  // threshold for long press

// Debounce bookkeeping
int lastRaw = HIGH;
unsigned long lastBounceTime = 0;
bool stablePressed = false; // debounced state: true when pressed
bool prevStablePressed = false;

// Gesture bookkeeping
unsigned long pressStart = 0;
bool longPressEmitted = false;

bool waitingForSecondClick = false;
unsigned long lastReleaseTime = 0;

void setup() {
  Serial.begin(115200);
  if (BUTTON_ACTIVE_HIGH) {
    pinMode(BUTTON_PIN, INPUT); // using external pull-down
  } else {
    pinMode(BUTTON_PIN, INPUT_PULLUP); // using internal pull-up
  }
  Serial.println("Button gestures ready (set BUTTON_ACTIVE_HIGH accordingly)");
}

void loop() {
  // --- Read raw pin and normalize to 'rawPressed' boolean ---
  int raw = digitalRead(BUTTON_PIN);
  bool rawPressed = (BUTTON_ACTIVE_HIGH ? (raw == HIGH) : (raw == LOW));

  // --- Debounce ---
  if (raw != lastRaw) {
    lastBounceTime = millis();
  }

  if ((millis() - lastBounceTime) > DEBOUNCE_TIME) {
    stablePressed = rawPressed;
  }

  lastRaw = raw;

  // --- Detect edges (press / release) using stablePressed and prevStablePressed ---
  if (stablePressed && !prevStablePressed) {
    // PRESSED event
    pressStart = millis();
    longPressEmitted = false;
    // If we were waiting for second click, we should NOT cancel waiting here —
    // we only decide on release. (Double-click logic handled on release.)
  }

  // While button held: check long-press
  if (stablePressed && !longPressEmitted) {
    if (millis() - pressStart >= LONG_PRESS_MS) {
      // Long press detected (while still holding)
      Serial.println("LONG PRESS DETECTED -> START!");
      longPressEmitted = true;
      // Cancel any pending single-click wait because long-press is a distinct action
      waitingForSecondClick = false;
    }
  }

  // Release event
  if (!stablePressed && prevStablePressed) {
    unsigned long pressDuration = millis() - pressStart;

    if (longPressEmitted) {
      // We had emitted a long press earlier; user released -> stop action
      Serial.println("LONG PRESS RELEASED -> STOP!");
      longPressEmitted = false;
    } else {
      // Short press (candidate for single/double click)
      if (waitingForSecondClick) {
        // We are in the window waiting for second click
        if (millis() - lastReleaseTime <= DOUBLE_CLICK_MS) {
          Serial.println("DOUBLE CLICK Detected");
          waitingForSecondClick = false;
        } else {
          // Previous wait expired (rare to reach here since single click would have fired),
          // treat this release as the start of a new waiting window:
          waitingForSecondClick = true;
          lastReleaseTime = millis();
        }
      } else {
        // First click -> start waiting for possible double click
        waitingForSecondClick = true;
        lastReleaseTime = millis();
        // Don't declare SINGLE CLICK yet — wait for DOUBLE_CLICK_MS to expire
      }
    }
  }

  // If waiting for second click and the timeout expired -> it's a single click
  if (waitingForSecondClick && (millis() - lastReleaseTime > DOUBLE_CLICK_MS)) {
    Serial.println("SINGLE CLICK Detected");
    waitingForSecondClick = false;
  }

  prevStablePressed = stablePressed;
}
