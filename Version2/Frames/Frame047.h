#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 047 = animation frame F=46.
#define FRAME047_INDEX 46

inline void frame047Begin() { V2FrameEngine::clear(); }
inline void frame047SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame047Submit() { V2FrameEngine::submit(); }
