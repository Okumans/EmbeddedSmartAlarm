#include "../../include/shared/validation_state.h"

// Definitions compiled into the gateway_esp32 build (project uses a source
// filter that only includes `src/gateway_esp32/` for this environment).
bool awaitingValidation = false;
String awaitingResumeFile = "";
float awaitingPrevVolume = 1.0f;
