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

// Match the old firmware's 8x8 layer display buffer.
// displayBuffer[Z][register] contains the already-mapped 8 column bytes
// for one Z layer. A complete frame is copied atomically into it.
static volatile uint8_t displayBuffer[8][8];

// Current multiplexed layer. Accessed only by the refresh ISR.
static volatile uint8_t currentLayer = 0;

// Same brightness scale used by the existing firmware.
static volatile uint8_t globalBrightness = 4;
static volatile uint8_t brightnessAccumulator[8] = {0,0,0,0,0,0,0,0};

// Accept the already-mapped 64-byte frame produced by FrameEngine.
// frame[Z*8 + register] is the exact format consumed by refreshDisplay().
// The copy is atomic with respect to the Timer2 refresh ISR.
inline void submitFrame(const uint8_t *frame) {
  if (!frame) return;

  noInterrupts();
  memcpy((void *)displayBuffer, frame, 64);
  interrupts();
}

inline void setBrightness(uint8_t brightness) {
  globalBrightness = (brightness > 8) ? 8 : brightness;
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

// Refresh one Z layer exactly through the old firmware's 8x8 layer buffer.
inline void refreshDisplay() {
  const uint8_t layer = currentLayer;

  // Blank all outputs before changing layer/column data to prevent ghosting.
  shiftByteFast(0);
  for (int8_t reg = 7; reg >= 0; reg--) shiftByteFast(0);
  latchFast();

  brightnessAccumulator[layer] += globalBrightness;
  const bool layerEnabled = brightnessAccumulator[layer] >= 8;
  if (layerEnabled) brightnessAccumulator[layer] -= 8;

  // Layer byte: one-hot layer selection, same as the old firmware.
  shiftByteFast(layerEnabled ? (1 << layer) : 0);

  // The buffer already contains the physical register bytes, so the ISR
  // performs no 512-voxel reconstruction. This matches the old path.
  for (int8_t reg = 7; reg >= 0; reg--) {
    shiftByteFast(displayBuffer[layer][reg]);
  }

  latchFast();
  currentLayer = (layer + 1) & 7;
}

// Call this from the V2 Timer2 compare ISR.
inline void onTimer2Compare() {
  refreshDisplay();
}

} // namespace V2DisplayEngine
