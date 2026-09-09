#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 020 = animation frame F=19.
#define FRAME020_INDEX 19

inline void frame020Begin() { V2FrameEngine::clear(); }
inline void frame020SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame020Submit() { V2FrameEngine::submit(); }
