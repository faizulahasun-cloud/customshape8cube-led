#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 022 = animation frame F=21.
#define FRAME022_INDEX 21

inline void frame022Begin() { V2FrameEngine::clear(); }
inline void frame022SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame022Submit() { V2FrameEngine::submit(); }
