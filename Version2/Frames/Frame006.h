#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 006 = animation frame F=5.
#define FRAME006_INDEX 5

inline void frame006Begin() { V2FrameEngine::clear(); }
inline void frame006SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame006Submit() { V2FrameEngine::submit(); }
