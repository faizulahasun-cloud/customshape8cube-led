#pragma once

// V2 Frame 021
// 512-voxel frame formula.
// Coordinates: X=0..7, Y=0..7, Z=0..7.
// Frame index: F=20.

#define FRAME021_INDEX 20

// Returns true when voxel (X,Y,Z) is ON for this frame.
inline bool frame021Voxel(uint8_t X, uint8_t Y, uint8_t Z) {
    const uint8_t F = FRAME021_INDEX;
    (void)F;
    (void)X;
    (void)Y;
    (void)Z;
    return false;
}
