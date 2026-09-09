#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 039 = animation frame F=38.
#define FRAME039_INDEX 38

inline void frame039Begin() { V2FrameEngine::clear(); }
inline void frame039SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame039Submit() { V2FrameEngine::submit(); }
