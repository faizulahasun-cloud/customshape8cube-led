#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 016 = animation frame F=15.
#define FRAME016_INDEX 15

inline void frame016Begin() { V2FrameEngine::clear(); }
inline void frame016SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame016Submit() { V2FrameEngine::submit(); }
