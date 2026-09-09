#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 032 = animation frame F=31.
#define FRAME032_INDEX 31

inline void frame032Begin() { V2FrameEngine::clear(); }
inline void frame032SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame032Submit() { V2FrameEngine::submit(); }
