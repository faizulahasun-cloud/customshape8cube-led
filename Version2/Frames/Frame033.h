#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 033 = animation frame F=32.
#define FRAME033_INDEX 32

inline void frame033Begin() { V2FrameEngine::clear(); }
inline void frame033SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame033Submit() { V2FrameEngine::submit(); }
