#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 048 = animation frame F=47.
#define FRAME048_INDEX 47

inline void frame048Begin() { V2FrameEngine::clear(); }
inline void frame048SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame048Submit() { V2FrameEngine::submit(); }
