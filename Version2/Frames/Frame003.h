#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 003 = animation frame F=2.
#define FRAME003_INDEX 2

inline void frame003Begin() { V2FrameEngine::clear(); }
inline void frame003SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame003Submit() { V2FrameEngine::submit(); }
