#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 019 = animation frame F=18.
#define FRAME019_INDEX 18

inline void frame019Begin() { V2FrameEngine::clear(); }
inline void frame019SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame019Submit() { V2FrameEngine::submit(); }
