#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 040 = animation frame F=39.
#define FRAME040_INDEX 39

inline void frame040Begin() { V2FrameEngine::clear(); }
inline void frame040SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame040Submit() { V2FrameEngine::submit(); }
