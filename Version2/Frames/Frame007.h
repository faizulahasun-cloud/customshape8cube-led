#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 007 = animation frame F=6.
#define FRAME007_INDEX 6

inline void frame007Begin() { V2FrameEngine::clear(); }
inline void frame007SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame007Submit() { V2FrameEngine::submit(); }
