#pragma once

#include <Arduino.h>
#include "../FunctionConversion/FunctionConversionEngine.h"
#include "../Frames/FrameEngine.h"

// V2 Animation Engine
// -------------------
// Takes the complete compiled function from the Function Conversion Engine,
// evaluates it for all 512 voxels for the current F, and converts the result
// into the compatible 64-byte frame data required by the Frame Engine.
//
// Frame data format:
//   byte = Z*8 + Y
//   bit  = X
//   64 bytes = 512 voxels
//
// The function is NOT hard-coded here. F changes from frame to frame so the
// same received function generates the complete animation.

namespace V2Animation {

static const uint8_t TOTAL_FRAMES = 50;
static uint8_t currentFrame = 0;

// Frame callback used by the Function Conversion Engine evaluator.
inline bool evaluateCurrentVoxel(uint8_t X, uint8_t Y, uint8_t Z) {
  return V2FunctionConversion::evaluate(X, Y, Z, currentFrame);
}

// Convert the current function at a specific F into one compatible frame.
inline bool generateFrame(uint8_t frameIndex) {
  if (!V2FunctionConversion::isFunctionValid()) return false;

  currentFrame = frameIndex % TOTAL_FRAMES;
  V2FrameEngine::clear();

  for (uint8_t Z = 0; Z < 8; Z++) {
    for (uint8_t Y = 0; Y < 8; Y++) {
      for (uint8_t X = 0; X < 8; X++) {
        if (evaluateCurrentVoxel(X, Y, Z)) {
          V2FrameEngine::setVoxel(X, Y, Z, true);
        }
      }
    }
  }

  // The Frame Engine now owns a complete 64-byte compatible frame.
  V2FrameEngine::submit();
  return true;
}

// Generate the next animation frame. The same received function is reused;
// only F advances.
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
