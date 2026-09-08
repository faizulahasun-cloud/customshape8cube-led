#pragma once

#include <Arduino.h>
#include <avr/interrupt.h>

// V2 display engine
// -----------------
// Logical cube: X=0..7, Y=0..7, Z=0..7.
// Physical LED numbering: LED = Z*64 + Y*8 + X + 1.
//
// This file is self-contained. V2 does NOT depend on Main.ino or any other
// file for the physical column mapping.
//
// Hardware mapping copied from the current working firmware as the V2
// reference mapping:
//   Column index = Y*8 + X
//   Register 1: Y=0, bits 0..7 = X=0..7
//   Register 2: Y=1, bits 0..7 = X=0..7
//   Register 3: Y=2, bits 0..7 = X=0..7
//   Register 4: Y=3, bits 0..7 = X=0..7
//   Register 5: Y=4, bits 0..7 = X=0..7
//   Register 6: Y=5, bits 0..7 = X=0..7
//   Register 7: Y=6, bits 0..7 = X=0..7
//   Register 8: Y=7, bits 0..7 = X=0..7
//
// Physical pins used by the existing hardware:
//   DATA  = D11 / PB3
//   CLOCK = D13 / PB5
//   LATCH = D12 / PB4
//   8 column shift-register bytes + 1 layer byte

namespace V2DisplayEngine {

struct ColumnMap {
  uint8_t reg;
  uint8_t bit;
};

// Complete V2 column mapping. Do not rely on Main.ino for this table.
static const ColumnMap COLUMN_MAP[64] = {
  {1,0},{1,1},{1,2},{1,3},{1,4},{1,5},{1,6},{1,7},
  {2,0},{2,1},{2,2},{2,3},{2,4},{2,5},{2,6},{2,7},
  {3,0},{3,1},{3,2},{3,3},{3,4},{3,5},{3,6},{3,7},
  {4,0},{4,1},{4,2},{4,3},{4,4},{4,5},{4,6},{4,7},
  {5,0},{5,1},{5,2},{5,3},{5,4},{5,5},{5,6},{5,7},
  {6,0},{6,1},{6,2},{6,3},{6,4},{6,5},{6,6},{6,7},
  {7,0},{7,1},{7,2},{7,3},{7,4},{7,5},{7,6},{7,7},
  {8,0},{8,1},{8,2},{8,3},{8,4},{8,5},{8,6},{8,7}
};

inline uint8_t columnIndex(uint8_t x, uint8_t y) {
  return y * 8 + x;
}

static volatile uint8_t voxelBuffer[2][8][8][8];
static volatile uint8_t activeBuffer = 0;

// Current multiplexed layer. Accessed only by the refresh ISR.
static volatile uint8_t currentLayer = 0;

// Same brightness scale used by the existing firmware.
static volatile uint8_t globalBrightness = 4;
static volatile uint8_t brightnessAccumulator[8] = {0,0,0,0,0,0,0,0};

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
// mapped 8 column-register bytes and transfer them to the shift registers.
inline void refreshDisplay() {
  const uint8_t layer = currentLayer;
  const uint8_t buffer = activeBuffer;

  // Blank all outputs before changing layer/column data to prevent ghosting.
  shiftByteFast(0);
  for (int8_t reg = 7; reg >= 0; reg--) shiftByteFast(0);
  latchFast();

  brightnessAccumulator[layer] += globalBrightness;
  const bool layerEnabled = brightnessAccumulator[layer] >= 8;
  if (layerEnabled) brightnessAccumulator[layer] -= 8;

  // Layer byte: one-hot layer selection, same as the existing hardware logic.
  shiftByteFast(layerEnabled ? (1 << layer) : 0);

  // Build each physical shift-register byte from the explicit V2 mapping.
  // COLUMN_MAP index = Y*8+X; reg/bit determine the physical output.
  uint8_t columnBytes[8] = {0,0,0,0,0,0,0,0};

  for (uint8_t y = 0; y < 8; y++) {
    for (uint8_t x = 0; x < 8; x++) {
      if (!voxelBuffer[buffer][layer][y][x]) continue;

      const uint8_t c = columnIndex(x, y);
      const ColumnMap map = COLUMN_MAP[c];
      if (map.reg >= 1 && map.reg <= 8 && map.bit <= 7) {
        columnBytes[map.reg - 1] |= (uint8_t)(1 << map.bit);
      }
    }
  }

  // The physical shift chain receives register 8 first, then 7 ... 1.
  for (int8_t reg = 7; reg >= 0; reg--) {
    shiftByteFast(columnBytes[reg]);
  }

  latchFast();
  currentLayer = (layer + 1) & 7;
}

// Call this from the V2 Timer2 compare ISR.
inline void onTimer2Compare() {
  refreshDisplay();
}

} // namespace V2DisplayEngine
