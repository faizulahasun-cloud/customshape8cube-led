#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 024 = animation frame F=23.
#define FRAME024_INDEX 23

inline void frame024Begin() { V2FrameEngine::clear(); }
inline void frame024SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame024Submit() { V2FrameEngine::submit(); }
