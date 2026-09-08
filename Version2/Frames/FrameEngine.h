#pragma once

#include <Arduino.h>
#include "../Multiplexing/DisplayEngine.h"

// V2 Frame Engine
// ---------------
// Receives one complete 512-voxel frame as 64 bytes (8 bits per byte).
// Byte index = Z*8 + Y; bit index = X.
// Therefore each frame is exactly 64 bytes.
//
// The Frame Engine does not know how the animation was generated. It only
// accepts compatible frame data and hands it to the existing V2 display
// engine.

namespace V2FrameEngine {

static uint8_t frameData[64];

inline void clear() {
  memset(frameData, 0, sizeof(frameData));
}

inline void setVoxel(uint8_t X, uint8_t Y, uint8_t Z, bool on) {
  if (X > 7 || Y > 7 || Z > 7) return;
  const uint8_t index = Z * 8 + Y;
  const uint8_t mask = (uint8_t)(1 << X);
  if (on) frameData[index] |= mask;
  else frameData[index] &= (uint8_t)~mask;
}

inline bool getVoxel(uint8_t X, uint8_t Y, uint8_t Z) {
  if (X > 7 || Y > 7 || Z > 7) return false;
  return (frameData[Z * 8 + Y] & (uint8_t)(1 << X)) != 0;
}

inline void setData(const uint8_t *data) {
  if (!data) return;
  memcpy(frameData, data, sizeof(frameData));
}

inline const uint8_t *data() {
  return frameData;
}

inline void submit() {
  V2DisplayEngine::buildFrame(
    [](uint8_t X, uint8_t Y, uint8_t Z) -> bool {
      return V2FrameEngine::getVoxel(X, Y, Z);
    }
  );
}

} // namespace V2FrameEngine
