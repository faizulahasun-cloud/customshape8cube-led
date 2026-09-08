#pragma once

// V2 Frame 010
// 512-voxel frame formula.
// Coordinates: X=0..7, Y=0..7, Z=0..7.
// Frame index: F=9.

#define FRAME010_INDEX 9

// Returns true when voxel (X,Y,Z) is ON for this frame.
inline bool frame010Voxel(uint8_t X, uint8_t Y, uint8_t Z) {
    const uint8_t F = FRAME010_INDEX;
    (void)F;
    (void)X;
    (void)Y;
    (void)Z;
    return false;
}
