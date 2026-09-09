#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 026 = animation frame F=25.
#define FRAME026_INDEX 25

inline void frame026Begin() { V2FrameEngine::clear(); }
inline void frame026SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame026Submit() { V2FrameEngine::submit(); }
