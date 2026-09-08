#pragma once

// V2 Frame 047
// 512-voxel frame formula.
// Coordinates: X=0..7, Y=0..7, Z=0..7.
// Frame index: F=46.

#define FRAME047_INDEX 46

// Returns true when voxel (X,Y,Z) is ON for this frame.
inline bool frame047Voxel(uint8_t X, uint8_t Y, uint8_t Z) {
    const uint8_t F = FRAME047_INDEX;
    (void)F;
    (void)X;
    (void)Y;
    (void)Z;
    return false;
}
