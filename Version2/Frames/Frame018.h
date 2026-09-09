#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 018 = animation frame F=17.
#define FRAME018_INDEX 17

inline void frame018Begin() { V2FrameEngine::clear(); }
inline void frame018SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame018Submit() { V2FrameEngine::submit(); }
