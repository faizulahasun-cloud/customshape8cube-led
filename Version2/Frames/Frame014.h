#pragma once
#include <Arduino.h>
#include "FrameEngine.h"

// V2 Frame 014 = animation frame F=13.
#define FRAME014_INDEX 13

inline void frame014Begin() { V2FrameEngine::clear(); }
inline void frame014SetLED(uint16_t ledNumber) { V2FrameEngine::setLED(ledNumber); }
inline void frame014Submit() { V2FrameEngine::submit(); }
