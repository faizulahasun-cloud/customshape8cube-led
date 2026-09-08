#pragma once

// V2 Frame 008
// 512-voxel frame formula.
// Coordinates: X=0..7, Y=0..7, Z=0..7.
// Frame index: F=7.

#define FRAME008_INDEX 7

// Returns true when voxel (X,Y,Z) is ON for this frame.
inline bool frame008Voxel(uint8_t X, uint8_t Y, uint8_t Z) {
    const uint8_t F = FRAME008_INDEX;
    (void)F;
    (void)X;
    (void)Y;
    (void)Z;
    return false;
}
