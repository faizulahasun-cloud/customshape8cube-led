#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 009 = animation frame F=8.
#define FRAME009_INDEX 8

inline void frame009Begin() { V2FrameEngine::clear(); }
inline void frame009SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame009Submit() { V2FrameEngine::submit(); }
