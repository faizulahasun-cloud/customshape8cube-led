#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 045 = animation frame F=44.
#define FRAME045_INDEX 44

inline void frame045Begin() { V2FrameEngine::clear(); }
inline void frame045SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame045Submit() { V2FrameEngine::submit(); }
