#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 037 = animation frame F=36.
#define FRAME037_INDEX 36

inline void frame037Begin() { V2FrameEngine::clear(); }
inline void frame037SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame037Submit() { V2FrameEngine::submit(); }
