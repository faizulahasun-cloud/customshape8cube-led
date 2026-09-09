#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 025 = animation frame F=24.
#define FRAME025_INDEX 24

inline void frame025Begin() { V2FrameEngine::clear(); }
inline void frame025SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame025Submit() { V2FrameEngine::submit(); }
