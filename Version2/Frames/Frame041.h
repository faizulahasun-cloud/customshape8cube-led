#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 041 = animation frame F=40.
#define FRAME041_INDEX 40

inline void frame041Begin() { V2FrameEngine::clear(); }
inline void frame041SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame041Submit() { V2FrameEngine::submit(); }
