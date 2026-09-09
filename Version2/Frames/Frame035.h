#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 035 = animation frame F=34.
#define FRAME035_INDEX 34

inline void frame035Begin() { V2FrameEngine::clear(); }
inline void frame035SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame035Submit() { V2FrameEngine::submit(); }
