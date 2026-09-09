#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 012 = animation frame F=11.
#define FRAME012_INDEX 11

inline void frame012Begin() { V2FrameEngine::clear(); }
inline void frame012SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame012Submit() { V2FrameEngine::submit(); }
