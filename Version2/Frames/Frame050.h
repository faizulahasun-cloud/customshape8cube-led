#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 050 = animation frame F=49.
#define FRAME050_INDEX 49

inline void frame050Begin() { V2FrameEngine::clear(); }
inline void frame050SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame050Submit() { V2FrameEngine::submit(); }
