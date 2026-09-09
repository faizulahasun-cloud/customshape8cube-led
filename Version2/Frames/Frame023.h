#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 023 = animation frame F=22.
#define FRAME023_INDEX 22

inline void frame023Begin() { V2FrameEngine::clear(); }
inline void frame023SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame023Submit() { V2FrameEngine::submit(); }
