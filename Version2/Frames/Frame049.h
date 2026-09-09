#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 049 = animation frame F=48.
#define FRAME049_INDEX 48

inline void frame049Begin() { V2FrameEngine::clear(); }
inline void frame049SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame049Submit() { V2FrameEngine::submit(); }
