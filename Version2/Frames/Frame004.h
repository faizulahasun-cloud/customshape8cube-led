#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 004 = animation frame F=3.
#define FRAME004_INDEX 3

inline void frame004Begin() { V2FrameEngine::clear(); }
inline void frame004SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame004Submit() { V2FrameEngine::submit(); }
