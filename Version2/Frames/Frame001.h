#pragma once

// V2 Frame 001
// 512-voxel frame formula.
// Coordinates: X=0..7, Y=0..7, Z=0..7.
// Frame index: F=0.

#define FRAME001_INDEX 0

// Returns true when voxel (X,Y,Z) is ON for this frame.
inline bool frame001Voxel(uint8_t X, uint8_t Y, uint8_t Z) {
    const uint8_t F = FRAME001_INDEX;
    (void)F;
    (void)X;
    (void)Y;
    (void)Z;
    return false;
}
