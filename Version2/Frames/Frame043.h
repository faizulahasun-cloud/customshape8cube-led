#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 043 = animation frame F=42.
#define FRAME043_INDEX 42

inline void frame043Begin() { V2FrameEngine::clear(); }
inline void frame043SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame043Submit() { V2FrameEngine::submit(); }
