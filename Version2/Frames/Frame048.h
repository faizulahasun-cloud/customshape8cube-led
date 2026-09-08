#pragma once

// V2 Frame 048
// 512-voxel frame formula.
// Coordinates: X=0..7, Y=0..7, Z=0..7.
// Frame index: F=47.

#define FRAME048_INDEX 47

// Returns true when voxel (X,Y,Z) is ON for this frame.
inline bool frame048Voxel(uint8_t X, uint8_t Y, uint8_t Z) {
    const uint8_t F = FRAME048_INDEX;
    (void)F;
    (void)X;
    (void)Y;
    (void)Z;
    return false;
}
