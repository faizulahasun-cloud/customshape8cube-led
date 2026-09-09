#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 034 = animation frame F=33.
#define FRAME034_INDEX 33

inline void frame034Begin() { V2FrameEngine::clear(); }
inline void frame034SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame034Submit() { V2FrameEngine::submit(); }
