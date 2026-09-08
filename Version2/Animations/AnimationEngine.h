#pragma once

#include <Arduino.h>
#include "../FunctionConversion/FunctionConversionEngine.h"
#include "../Multiplexing/DisplayEngine.h"

// V2 Animation Engine
// -------------------
// Converts the complete function received by the Function Conversion Engine
// into frame data for the V2 frame/display pipeline.
//
// The function itself is NOT hard-coded into this engine. The compiled
// function is evaluated for every voxel at the current animation frame F.
// Each completed evaluation pass produces one 512-voxel frame.
//
// Coordinates: X=0..7, Y=0..7, Z=0..7.
// Frame index F starts at 0 and advances once per generated frame.
//
// This engine does not perform physical LED mapping, shift-register control,
// brightness control, or Bluetooth communication.

namespace V2Animation {

static const uint8_t TOTAL_FRAMES = 50;

// Current animation frame number.
static uint8_t currentFrame = 0;

// Generate one frame directly from the compiled function.
// The resulting 512 ON/OFF voxel values are handed to the V2 display/frame
// pipeline through DisplayEngine::buildFrame().
inline bool generateFrame(uint8_t frameIndex) {
    if (!V2FunctionConversion::isFunctionValid()) return false;

    const V2FunctionConversion::Instruction *program =
        V2FunctionConversion::compiledFunction();
    const uint8_t programLength = V2FunctionConversion::compiledLength();

    // Keep the function representation owned by FunctionConversionEngine;
    // Animation Engine only supplies X,Y,Z,F and converts the result into
    // frame voxel data.
    V2Display::buildFrame(
        [program, programLength, frameIndex](uint8_t X, uint8_t Y, uint8_t Z) -> bool {
            (void)programLength;
            return V2FunctionConversion::evaluateProgram(program, X, Y, Z, frameIndex);
        }
    );

    currentFrame = frameIndex;
    return true;
}

// Generate the next frame and wrap after the current 50-frame animation.
inline bool generateNextFrame() {
    if (!V2FunctionConversion::isFunctionValid()) return false;

    bool ok = generateFrame(currentFrame);
    currentFrame++;
    if (currentFrame >= TOTAL_FRAMES) currentFrame = 0;
    return ok;
}

inline void reset() {
    currentFrame = 0;
}

inline uint8_t frameIndex() {
    return currentFrame;
}

} // namespace V2Animation
