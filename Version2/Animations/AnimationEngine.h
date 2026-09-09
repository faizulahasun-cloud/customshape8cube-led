#pragma once

#include <Arduino.h>
#include "../FunctionConversion/FunctionConversionEngine.h"
#include "../Frames/FrameEngine.h"
#include "../LEDs/LEDDefinitions.h"

// V2 Animation Engine
// -------------------
// Evaluates the compiled function for every physical LED definition.
// LED001.h ... LED512.h are the source of truth for the physical X/Y/Z
// position of each LED. No LED-number formula is used here.
//
// One frame is generated at a time:
//   Function Conversion -> individual LED definitions -> Frame Engine
//   -> Display Engine
// The completed frame is submitted immediately; frames are not stored as
// 50 simultaneous RAM buffers and the .h source files are not rewritten.

namespace V2Animation {

static const uint8_t TOTAL_FRAMES = 50;
static uint8_t currentFrame = 0;

// Evaluate the compiled function at the current animation frame F.
inline bool evaluateCurrentVoxel(uint8_t X, uint8_t Y, uint8_t Z) {
  return V2FunctionConversion::evaluate(X, Y, Z, currentFrame);
}

// Generate exactly one frame from the individual physical LED definitions.
// LED number is the position in LED001.h ... LED512.h.
inline bool generateFrame(uint8_t frameIndex) {
  if (!V2FunctionConversion::isFunctionValid()) return false;

  currentFrame = frameIndex % TOTAL_FRAMES;
  V2FrameEngine::clear();

  // Walk the physical LED definitions directly. Each LED file supplies the
  // coordinates used for evaluating the user's function.
  for (uint16_t ledIndex = 0; ledIndex < 512; ledIndex++) {
    const V2LEDDefinitions::Definition &led =
      V2LEDDefinitions::DEFINITIONS[ledIndex];

    if (!evaluateCurrentVoxel(led.x, led.y, led.z)) continue;

    // ledIndex 0 = LED001, ledIndex 511 = LED512.
    V2FrameEngine::setLED(ledIndex + 1);
  }

  // Pass this completed frame immediately to the Display Engine.
  V2FrameEngine::submit();
  return true;
}

inline void reset() {
  currentFrame = 0;
}

// Start at animation frame F=0.
inline bool start() {
  reset();
  return generateFrame(0);
}

// Generate the next frame sequentially: 0, 1, 2, ... 49, then repeat.
inline bool generateNextFrame() {
  const uint8_t nextFrame = (currentFrame + 1) % TOTAL_FRAMES;
  return generateFrame(nextFrame);
}

inline uint8_t frameIndex() {
  return currentFrame;
}

} // namespace V2Animation
