#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 038 = animation frame F=37.
#define FRAME038_INDEX 37

inline void frame038Begin() { V2FrameEngine::clear(); }
inline void frame038SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame038Submit() { V2FrameEngine::submit(); }
