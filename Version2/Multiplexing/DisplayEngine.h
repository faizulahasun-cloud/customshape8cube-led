#pragma once

#include <Arduino.h>
#include <avr/interrupt.h>

// V2 display engine
// -----------------
// Logical cube: X=0..7, Y=0..7, Z=0..7.
// Physical LED numbering: LED = Z*64 + Y*8 + X + 1.
//
// The frame generator writes the inactive voxel buffer.  After the complete
// 512-voxel frame is generated, the buffers are swapped atomically.  The
// refresh ISR reads only the active buffer, so it never displays a partially
// generated frame.
//
// The multiplexing output follows the existing V1 hardware arrangement:
//   - DATA  = D11 / PB3
//   - CLOCK = D13 / PB5
//   - LATCH = D12 / PB4
//   - 8 column shift-register bytes
//   - 1 layer byte
//
// Column order remains Y*8+X, matching the existing COLUMN_MAP in the main
// firmware. Layer Z is selected one layer at a time.

namespace V2DisplayEngine {

static volatile uint8_t voxelBuffer[2][8][8][8];
static volatile uint8_t activeBuffer = 0;

// Current multiplexed layer. Accessed only by the refresh ISR.
static volatile uint8_t currentLayer = 0;

// Same brightness scale used by the existing firmware.
static volatile uint8_t globalBrightness = 4;
static volatile uint8_t brightnessAccumulator[8] = {0, 0, 0, 0, 0, 0, 0, 0};

using FrameVoxelFunction = bool (*)(uint8_t X, uint8_t Y, uint8_t Z);

inline void clearBuffer(uint8_t bufferIndex) {
  memset((void *)voxelBuffer[bufferIndex], 0, 512);
}

// Generate one complete 512-voxel frame into the inactive buffer.
// The active display buffer is untouched while this runs.
inline void buildFrame(FrameVoxelFunction frameFunction) {
  const uint8_t inactive = activeBuffer ^ 1;

  for (uint8_t z = 0; z < 8; z++) {
    for (uint8_t y = 0; y < 8; y++) {
      for (uint8_t x = 0; x < 8; x++) {
        voxelBuffer[inactive][z][y][x] = frameFunction(x, y, z) ? 1 : 0;
      }
    }
  }

  // Swap only after all 512 voxels are complete.
  noInterrupts();
  activeBuffer = inactive;
  interrupts();
}

inline void setBrightness(uint8_t brightness) {
  globalBrightness = (brightness > 8) ? 8 : brightness;
}

inline uint8_t getActiveBuffer() {
  return activeBuffer;
}

inline void setLayer(uint8_t layer) {
  currentLayer = layer & 7;
}

inline uint8_t getLayer() {
  return currentLayer;
}

// Existing V1 fast shift-register timing and bit order.
inline void shiftByteFast(uint8_t value) {
  for (int8_t bit = 7; bit >= 0; bit--) {
    if (value & (1 << bit)) PORTB |= _BV(PB3);
    else PORTB &= ~_BV(PB3);
    PORTB |= _BV(PB5);
    PORTB &= ~_BV(PB5);
  }
}

inline void latchFast() {
  PORTB |= _BV(PB4);
  PORTB &= ~_BV(PB4);
}

// Convert the selected Z layer of the active 512-voxel buffer into the
// existing 8 column-register bytes and transfer it to the shift registers.
inline void refreshDisplay() {
  const uint8_t layer = currentLayer;
  const uint8_t buffer = activeBuffer;

  // First transfer blanks all outputs to prevent ghosting while changing
  // the layer and column data.
  shiftByteFast(0);
  for (int8_t reg = 7; reg >= 0; reg--) shiftByteFast(0);
  latchFast();

  brightnessAccumulator[layer] += globalBrightness;
  const bool layerEnabled = brightnessAccumulator[layer] >= 8;
  if (layerEnabled) brightnessAccumulator[layer] -= 8;

  // Layer byte: exactly the same one-hot layer selection used by the
  // existing firmware.
  shiftByteFast(layerEnabled ? (1 << layer) : 0);

  // Existing column mapping is register 1..8, each containing 8 columns.
  // Register r corresponds to Y=r and bit X within that row.
  for (int8_t reg = 7; reg >= 0; reg--) {
    uint8_t columnByte = 0;
    const uint8_t y = (uint8_t)reg;

    for (uint8_t x = 0; x < 8; x++) {
      if (voxelBuffer[buffer][layer][y][x]) {
        columnByte |= (uint8_t)(1 << x);
      }
    }

    shiftByteFast(columnByte);
  }

  latchFast();

  currentLayer = (layer + 1) & 7;
}

// Call this from the existing TIMER2 compare ISR.
inline void onTimer2Compare() {
  refreshDisplay();
}

} // namespace V2DisplayEngine
