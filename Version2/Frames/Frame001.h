#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 001 = animation frame F=0.
#define FRAME001_INDEX 0

inline void frame001Begin() { V2FrameEngine::clear(); }
inline void frame001SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame001Submit() { V2FrameEngine::submit(); }
