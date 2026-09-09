#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 031 = animation frame F=30.
#define FRAME031_INDEX 30

inline void frame031Begin() { V2FrameEngine::clear(); }
inline void frame031SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame031Submit() { V2FrameEngine::submit(); }
