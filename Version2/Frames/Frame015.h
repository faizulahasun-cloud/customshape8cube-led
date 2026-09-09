#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 015 = animation frame F=14.
#define FRAME015_INDEX 14

inline void frame015Begin() { V2FrameEngine::clear(); }
inline void frame015SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame015Submit() { V2FrameEngine::submit(); }
