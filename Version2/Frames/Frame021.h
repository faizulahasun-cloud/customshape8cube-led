#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 021 = animation frame F=20.
#define FRAME021_INDEX 20

inline void frame021Begin() { V2FrameEngine::clear(); }
inline void frame021SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame021Submit() { V2FrameEngine::submit(); }
