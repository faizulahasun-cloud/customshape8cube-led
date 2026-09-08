#pragma once

// V2 Frame 005
// 512-voxel frame formula.
// Coordinates: X=0..7, Y=0..7, Z=0..7.
// Frame index: F=4.

#define FRAME005_INDEX 4

// Returns true when voxel (X,Y,Z) is ON for this frame.
inline bool frame005Voxel(uint8_t X, uint8_t Y, uint8_t Z) {
    const uint8_t F = FRAME005_INDEX;
    (void)F;
    (void)X;
    (void)Y;
    (void)Z;
    return false;
}
