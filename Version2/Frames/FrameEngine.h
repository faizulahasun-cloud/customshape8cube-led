#pragma once

#include <Arduino.h>
#include "../Multiplexing/DisplayEngine.h"

// V2 Frame Engine
// ---------------
// Receives physical LED positions from the Animation Engine and combines
// them into one complete 512-LED frame.
//
// Physical LED numbering:
//   LED = Z*64 + Y*8 + X + 1
//   LED001 = X0,Y0,Z0
//   LED512 = X7,Y7,Z7
//
// Frame data format:
//   byte = Z*8 + Y
//   bit  = X
//   64 bytes = 512 LEDs

namespace V2FrameEngine {

static uint8_t frameData[64];

inline void clear() {
  memset(frameData, 0, sizeof(frameData));
}

// Receive one physical LED position from the Animation Engine.
inline void setLED(uint16_t ledNumber) {
  if (ledNumber < 1 || ledNumber > 512) return;

  const uint16_t zeroBased = ledNumber - 1;
  const uint8_t Z = zeroBased / 64;
  const uint8_t remainder = zeroBased % 64;
  const uint8_t Y = remainder / 8;
  const uint8_t X = remainder % 8;

  const uint8_t index = Z * 8 + Y;
  frameData[index] |= (uint8_t)(1 << X);
}

// Retained for direct frame manipulation where needed.
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
