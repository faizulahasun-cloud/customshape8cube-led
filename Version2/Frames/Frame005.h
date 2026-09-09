#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 005 = animation frame F=4.
#define FRAME005_INDEX 4

inline void frame005Begin() { V2FrameEngine::clear(); }
inline void frame005SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame005Submit() { V2FrameEngine::submit(); }
