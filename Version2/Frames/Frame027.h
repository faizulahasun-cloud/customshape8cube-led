#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 027 = animation frame F=26.
#define FRAME027_INDEX 26

inline void frame027Begin() { V2FrameEngine::clear(); }
inline void frame027SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame027Submit() { V2FrameEngine::submit(); }
