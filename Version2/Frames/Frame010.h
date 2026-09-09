#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 010 = animation frame F=9.
#define FRAME010_INDEX 9

inline void frame010Begin() { V2FrameEngine::clear(); }
inline void frame010SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame010Submit() { V2FrameEngine::submit(); }
