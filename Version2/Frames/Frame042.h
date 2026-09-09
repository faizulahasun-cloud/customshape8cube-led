#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 042 = animation frame F=41.
#define FRAME042_INDEX 41

inline void frame042Begin() { V2FrameEngine::clear(); }
inline void frame042SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame042Submit() { V2FrameEngine::submit(); }
