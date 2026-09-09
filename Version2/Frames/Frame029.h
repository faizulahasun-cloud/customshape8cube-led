#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 029 = animation frame F=28.
#define FRAME029_INDEX 28

inline void frame029Begin() { V2FrameEngine::clear(); }
inline void frame029SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame029Submit() { V2FrameEngine::submit(); }
