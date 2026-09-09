#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 028 = animation frame F=27.
#define FRAME028_INDEX 27

inline void frame028Begin() { V2FrameEngine::clear(); }
inline void frame028SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame028Submit() { V2FrameEngine::submit(); }
