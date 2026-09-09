#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 011 = animation frame F=10.
#define FRAME011_INDEX 10

inline void frame011Begin() { V2FrameEngine::clear(); }
inline void frame011SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame011Submit() { V2FrameEngine::submit(); }
