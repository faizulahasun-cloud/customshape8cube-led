#pragma once

// V2 Frame 046
// 512-voxel frame formula.
// Coordinates: X=0..7, Y=0..7, Z=0..7.
// Frame index: F=45.

#define FRAME046_INDEX 45

// Returns true when voxel (X,Y,Z) is ON for this frame.
inline bool frame046Voxel(uint8_t X, uint8_t Y, uint8_t Z) {
    const uint8_t F = FRAME046_INDEX;
    (void)F;
    (void)X;
    (void)Y;
    (void)Z;
    return false;
}
