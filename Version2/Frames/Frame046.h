#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 046 = animation frame F=45.
#define FRAME046_INDEX 45

inline void frame046Begin() { V2FrameEngine::clear(); }
inline void frame046SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame046Submit() { V2FrameEngine::submit(); }
