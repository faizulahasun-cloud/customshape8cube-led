#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 008 = animation frame F=7.
#define FRAME008_INDEX 7

inline void frame008Begin() { V2FrameEngine::clear(); }
inline void frame008SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame008Submit() { V2FrameEngine::submit(); }
