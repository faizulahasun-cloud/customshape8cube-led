#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 002 = animation frame F=1.
#define FRAME002_INDEX 1

inline void frame002Begin() { V2FrameEngine::clear(); }
inline void frame002SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame002Submit() { V2FrameEngine::submit(); }
