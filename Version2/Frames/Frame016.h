#pragma once

// V2 Frame 016
// 512-voxel frame formula.
// Coordinates: X=0..7, Y=0..7, Z=0..7.
// Frame index: F=15.

#define FRAME016_INDEX 15

// Returns true when voxel (X,Y,Z) is ON for this frame.
inline bool frame016Voxel(uint8_t X, uint8_t Y, uint8_t Z) {
    const uint8_t F = FRAME016_INDEX;
    (void)F;
    (void)X;
    (void)Y;
    (void)Z;
    return false;
}
