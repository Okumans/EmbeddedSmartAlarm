#ifndef VALIDATION_STATE_H
#define VALIDATION_STATE_H

#include <Arduino.h>

// Indicates whether the system is currently awaiting validation result
extern bool awaitingValidation;

// The audio file to resume while waiting (e.g. the alarm sound file)
extern String awaitingResumeFile;

// Previous volume before switching to low-volume waiting
extern float awaitingPrevVolume;

#endif  // VALIDATION_STATE_H
