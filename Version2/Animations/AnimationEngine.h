#pragma once

#include <Arduino.h>
#include "../FunctionConversion/FunctionConversionEngine.h"
#include "../Frames/FrameEngine.h"

// V2 Animation Engine
// -------------------
// Takes the compiled function from the Function Conversion Engine and the
// current F value, evaluates every X,Y,Z position, determines the physical
// LED number that is ON, and passes that physical LED position to the
// Frame Engine.
//
// Physical LED numbering follows Version2/LEDs:
//   LED = Z*64 + Y*8 + X + 1
//   LED001 = X0,Y0,Z0
//   LED512 = X7,Y7,Z7

namespace V2Animation {

static const uint8_t TOTAL_FRAMES = 50;
static uint8_t currentFrame = 0;

// Evaluate the compiled function at the current animation frame F.
inline bool evaluateCurrentVoxel(uint8_t X, uint8_t Y, uint8_t Z) {
  return V2FunctionConversion::evaluate(X, Y, Z, currentFrame);
}

// Generate one frame from the compiled function.
// Every ON coordinate is converted to its physical LED number and passed
// individually to the Frame Engine.
inline bool generateFrame(uint8_t frameIndex) {
  if (!V2FunctionConversion::isFunctionValid()) return false;

  currentFrame = frameIndex % TOTAL_FRAMES;
  V2FrameEngine::clear();

  for (uint8_t Z = 0; Z < 8; Z++) {
    for (uint8_t Y = 0; Y < 8; Y++) {
      for (uint8_t X = 0; X < 8; X++) {
        if (!evaluateCurrentVoxel(X, Y, Z)) continue;

        // Physical LED number defined by Version2/LEDs/LED001...LED512.
        const uint16_t ledNumber =
          (uint16_t)Z * 64u + (uint16_t)Y * 8u + X + 1u;

        V2FrameEngine::setLED(ledNumber);
      }
    }
  }

  // Frame Engine now owns the complete 512-LED frame.
  V2FrameEngine::submit();
  return true;
}

// Start the current compiled function immediately from animation frame F=0.
// This is the entry point the runtime controller uses after a new function
// has been successfully compiled.
inline bool start() {
  reset();
  return generateFrame(0);
}

// Generate the next animation frame using the same compiled function.
inline bool generateNextFrame() {
  bool ok = generateFrame(currentFrame);
  currentFrame = (currentFrame + 1) % TOTAL_FRAMES;
  return ok;
}

inline void reset() {
  currentFrame = 0;
}

inline uint8_t frameIndex() {
  return currentFrame;
}

} // namespace V2Animation
