#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 030 = animation frame F=29.
#define FRAME030_INDEX 29

inline void frame030Begin() { V2FrameEngine::clear(); }
inline void frame030SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame030Submit() { V2FrameEngine::submit(); }
