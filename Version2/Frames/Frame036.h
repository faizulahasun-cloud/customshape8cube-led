#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 036 = animation frame F=35.
#define FRAME036_INDEX 35

inline void frame036Begin() { V2FrameEngine::clear(); }
inline void frame036SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame036Submit() { V2FrameEngine::submit(); }
