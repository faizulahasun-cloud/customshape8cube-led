#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 017 = animation frame F=16.
#define FRAME017_INDEX 16

inline void frame017Begin() { V2FrameEngine::clear(); }
inline void frame017SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame017Submit() { V2FrameEngine::submit(); }
