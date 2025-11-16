#ifndef AUDIO_DATA_H
#define AUDIO_DATA_H

#include <Arduino.h>

// Audio sample rate
#define AUDIO_SAMPLE_RATE 44100

// ============================================================================
// Alarm Melodies - stored as frequency/duration pairs
// ============================================================================

// Simple alarm beep pattern
const struct {
  uint16_t frequency;  // Hz
  uint16_t duration;   // ms
} PROGMEM alarm_beep[] = {{800, 200}, {0, 100}, {800, 200}, {0, 100},
                          {800, 200}, {0, 100}, {800, 200}, {0, 100},
                          {800, 200}, {0, 100}, {800, 200}, {0, 0}};

// "Morning" melody - gentle wake up
const struct {
  uint16_t frequency;
  uint16_t duration;
} PROGMEM morning_melody[] = {{523, 300}, {587, 300}, {659, 300},  {784, 300},
                              {880, 300}, {988, 300}, {1047, 600}, {988, 300},
                              {880, 300}, {784, 600}, {0, 0}};

// Classic alarm sound
const struct {
  uint16_t frequency;
  uint16_t duration;
} PROGMEM classic_alarm[] = {{1000, 500}, {800, 500},  {1000, 500},
                             {800, 500},  {1000, 500}, {800, 500},
                             {1000, 500}, {800, 500},  {0, 0}};

// Happy Birthday melody
const struct {
  uint16_t frequency;
  uint16_t duration;
} PROGMEM happy_birthday[] = {
    {262, 400},  {262, 200}, {294, 600}, {262, 600}, {349, 600}, {330, 1200},
    {262, 400},  {262, 200}, {294, 600}, {262, 600}, {392, 600}, {349, 1200},
    {262, 400},  {262, 200}, {523, 600}, {440, 600}, {349, 600}, {330, 600},
    {294, 1200}, {466, 400}, {466, 200}, {440, 600}, {349, 600}, {392, 600},
    {349, 1200}, {0, 0}};

// Do Re Mi scale
const struct {
  uint16_t frequency;
  uint16_t duration;
} PROGMEM do_re_mi[] = {{262, 400}, {294, 400}, {330, 400},
                        {349, 400}, {392, 400}, {440, 400},
                        {494, 400}, {523, 800}, {0, 0}};

// Frère Jacques / Brother John
const struct {
  uint16_t frequency;
  uint16_t duration;
} PROGMEM frere_jacques[] = {
    {262, 400}, {262, 400}, {262, 400}, {294, 400}, {330, 400}, {262, 400},
    {262, 400}, {262, 400}, {262, 400}, {294, 400}, {330, 400}, {262, 400},
    {262, 400}, {262, 400}, {294, 400}, {330, 400}, {349, 400}, {330, 400},
    {294, 400}, {262, 400}, {440, 800}, {440, 800}, {0, 0}};

// Star Wars Imperial March (simplified)
const struct {
  uint16_t frequency;
  uint16_t duration;
} PROGMEM imperial_march[] = {{392, 500},  {392, 500},  {392, 500}, {311, 350},
                              {466, 150},  {392, 500},  {311, 350}, {466, 150},
                              {392, 1000}, {587, 500},  {587, 500}, {587, 500},
                              {622, 350},  {466, 150},  {370, 500}, {311, 350},
                              {466, 150},  {392, 1000}, {0, 0}};

// Nokia ringtone
const struct {
  uint16_t frequency;
  uint16_t duration;
} PROGMEM nokia_tone[] = {{1319, 125}, {1175, 125}, {740, 250}, {831, 250},
                          {1109, 125}, {988, 125},  {622, 250}, {740, 250},
                          {988, 125},  {880, 125},  {554, 250}, {622, 250},
                          {740, 500},  {0, 0}};

// Success/notification sound
const struct {
  uint16_t frequency;
  uint16_t duration;
} PROGMEM success_sound[] = {
    {523, 100}, {659, 100}, {784, 100}, {1047, 300}, {0, 0}};

// Error/warning sound
const struct {
  uint16_t frequency;
  uint16_t duration;
} PROGMEM error_sound[] = {{200, 200}, {150, 200}, {100, 400}, {0, 0}};

// Button press beep
const struct {
  uint16_t frequency;
  uint16_t duration;
} PROGMEM button_beep[] = {{1000, 50}, {0, 0}};

// ============================================================================
// Melody enumeration for easy selection
// ============================================================================

enum MelodyType {
  MELODY_ALARM_BEEP,
  MELODY_MORNING,
  MELODY_CLASSIC_ALARM,
  MELODY_HAPPY_BIRTHDAY,
  MELODY_DO_RE_MI,
  MELODY_FRERE_JACQUES,
  MELODY_IMPERIAL_MARCH,
  MELODY_NOKIA,
  MELODY_SUCCESS,
  MELODY_ERROR,
  MELODY_BUTTON_BEEP
};

// Helper struct to access melodies
struct MelodyInfo {
  const void* data;
  const char* name;
};

// Melody lookup table
const MelodyInfo PROGMEM melodyTable[] = {{alarm_beep, "Alarm Beep"},
                                          {morning_melody, "Morning Melody"},
                                          {classic_alarm, "Classic Alarm"},
                                          {happy_birthday, "Happy Birthday"},
                                          {do_re_mi, "Do Re Mi"},
                                          {frere_jacques, "Frere Jacques"},
                                          {imperial_march, "Imperial March"},
                                          {nokia_tone, "Nokia Ringtone"},
                                          {success_sound, "Success"},
                                          {error_sound, "Error"},
                                          {button_beep, "Button Beep"}};

#endif  // AUDIO_DATA_H
