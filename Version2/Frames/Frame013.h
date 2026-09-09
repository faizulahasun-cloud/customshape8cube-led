#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 013 = animation frame F=12.
#define FRAME013_INDEX 12

inline void frame013Begin() { V2FrameEngine::clear(); }
inline void frame013SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame013Submit() { V2FrameEngine::submit(); }
