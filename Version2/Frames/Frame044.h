#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 044 = animation frame F=43.
#define FRAME044_INDEX 43

inline void frame044Begin() { V2FrameEngine::clear(); }
inline void frame044SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame044Submit() { V2FrameEngine::submit(); }
