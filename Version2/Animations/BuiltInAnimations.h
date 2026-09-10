#pragma once

#include <Arduino.h>
#include <avr/pgmspace.h>

// The original 27 built-in animations, expressed only as voxel predicates.
// AnimationEngine supplies X/Y/Z/F and sends the resulting voxels through
// the existing FrameXXX -> FrameEngine -> DisplayEngine pipeline.
namespace V2BuiltInAnimations {

inline bool isOuterRing(uint8_t x, uint8_t y) {
  return x == 0 || x == 7 || y == 0 || y == 7;
}

inline uint8_t perimeterIndex(uint8_t x, uint8_t y) {
  if (y == 0) return x;
  if (x == 7) return 7 + y;
  if (y == 7) return 21 - x;
  return 21 + (7 - y);
}

inline bool firecrackerVoxel(uint8_t f, uint8_t x, uint8_t y, uint8_t z) {
  if (f < 16) {
    uint8_t lZ = f / 2;
    if ((x == 3 || x == 4) && (y == 3 || y == 4)) {
      if (z == lZ) return true;
      if (f > 1 && z + 1 == lZ) return true;
    }
    return false;
  }

  uint8_t bF = f - 16;
  uint8_t d = bF / 3;
  if (d > 3) d = 3;
  if (z != 7) return false;

  int vx = (int)x - 3;
  int vy = (int)y - 3;
  if (vx == 0 && vy == 0) return d == 0;
  if (!(vx == 0 || vy == 0 || abs(vx) == abs(vy))) return false;
  return max(abs(vx), abs(vy)) == (int)d;
}

static const uint8_t SNAKE_DIRS[49] PROGMEM = {
  0, 5, 1, 1, 5, 1, 2, 4, 2, 0, 0, 2, 5, 2, 5, 1, 4,
  1, 1, 5, 3, 3, 0, 0, 3, 1, 3, 4, 4, 2, 4, 0, 0, 2,
  4, 3, 4, 4, 2, 5, 5, 3, 3, 1, 2, 2, 0, 3, 5
};

inline void snakePosition(uint8_t step, uint8_t &sx, uint8_t &sy, uint8_t &sz) {
  int8_t px = 3, py = 3, pz = 3;
  for (uint8_t s = 0; s < step; s++) {
    uint8_t d = pgm_read_byte(&SNAKE_DIRS[s % 49]);
    if (d == 0) px++;
    else if (d == 1) px--;
    else if (d == 2) py++;
    else if (d == 3) py--;
    else if (d == 4) pz++;
    else pz--;
  }
  sx = (uint8_t)px;
  sy = (uint8_t)py;
  sz = (uint8_t)pz;
}

inline bool snakeVoxel(uint8_t f, uint8_t x, uint8_t y, uint8_t z) {
  for (uint8_t k = 0; k < 8; k++) {
    uint8_t step = (uint8_t)((f + 50 - k) % 50);
    uint8_t sx, sy, sz;
    snakePosition(step, sx, sy, sz);
    if (x == sx && y == sy && z == sz) return true;
  }
  return false;
}

static const uint8_t HEART_MASK[8] PROGMEM = {
  0x66, 0xFF, 0xFF, 0x7E, 0x3C, 0x18, 0x18, 0x00
};

inline bool rotatingHeartVoxel(uint8_t f, uint8_t x, uint8_t y, uint8_t z) {
  if (y != 0 && y != 1) return false;

  uint8_t r = (f / 4) % 4;
  uint8_t u = 0, v = 0;
  if (r == 0) {
    u = x;
    v = z;
  } else if (r == 1) {
    u = z;
    v = 7 - x;
  } else if (r == 2) {
    u = 7 - x;
    v = 7 - z;
  } else {
    u = 7 - z;
    v = x;
  }

  return (pgm_read_byte(&HEART_MASK[v]) & (1 << u)) != 0;
}

inline bool voxel(uint8_t animation, uint8_t f, uint8_t x, uint8_t y, uint8_t z) {
  if (animation == 0) return z == (f % 8);
  if (animation == 1) return z == (7 - (f % 8));
  if (animation == 2) return x == (f % 8);
  if (animation == 3) return y == (f % 8);
  if (animation == 4) return x == y && y == z && x == (f % 8);
  if (animation == 5) return x == y && z == (7 - x) && x == (f % 8);
  if (animation == 6) return ((x + y + z + f) & 1) == 0;

  if (animation == 7) {
    uint8_t r = f % 5;
    int d = max(abs((int)x - 3), max(abs((int)y - 3), abs((int)z - 3)));
    return d == r;
  }

  if (animation == 8) {
    uint8_t r = 4 - (f % 5);
    int d = max(abs((int)x - 3), max(abs((int)y - 3), abs((int)z - 3)));
    return d == r;
  }

  if (animation == 9) {
    if (!(x == 3 || x == 4 || y == 3 || y == 4 || z == 3 || z == 4)) return false;
    return ((x + y + z + f) & 1) == 0;
  }

  if (animation == 10) {
    uint8_t w = (x + y + f) % 8;
    return z == w || z == ((w + 1) % 8);
  }

  if (animation == 11) {
    uint8_t s = (f / 2) % 8;
    if (s == 0) return x == 0;
    if (s == 1) return y == 7;
    if (s == 2) return x == 7;
    return y == 0;
  }

  if (animation == 12) {
    if (!isOuterRing(x, y)) return false;
    uint8_t p = perimeterIndex(x, y);
    return ((p + f) % 28) < 3;
  }

  if (animation == 13) {
    if (!isOuterRing(x, y)) return false;
    uint8_t p = perimeterIndex(x, y);
    return z == ((p + f) % 8);
  }

  if (animation == 14) {
    uint8_t h = (x * 3 + y * 5 + f) % 16;
    if (h >= 8) return false;
    uint8_t rz = 7 - h;
    return z == rz || (rz < 7 && z == rz + 1);
  }

  if (animation == 15) {
    int dx = abs((int)x - 3);
    int dy = abs((int)y - 3);
    if (dx <= 1 && dy <= 1) {
      if (z > ((f / 2) % 8)) return false;
      return ((x + y + f) & 1) != 0;
    }
    return false;
  }

  if (animation == 16) {
    if (!isOuterRing(x, y)) return false;
    uint8_t p = perimeterIndex(x, y);
    uint8_t o = (p + f) % 28;
    return z == (o % 8) || z == ((o + 1) % 8);
  }

  if (animation == 17) {
    if (!isOuterRing(x, y)) return false;
    uint8_t p = perimeterIndex(x, y);
    return z == ((p + f) % 8);
  }

  if (animation == 18) {
    uint8_t r = f % 8;
    int d = abs((int)x - 3) + abs((int)y - 3) + abs((int)z - 3);
    return d == r || d == r + 1;
  }

  if (animation == 19) {
    uint8_t r = f % 10;
    int d = min(abs((int)x - 3), abs((int)x - 4))
          + min(abs((int)y - 3), abs((int)y - 4))
          + min(abs((int)z - 3), abs((int)z - 4));
    return d == r || d == r + 1;
  }

  if (animation == 20) {
    uint8_t r = 9 - (f % 10);
    int d = min(abs((int)x - 3), abs((int)x - 4))
          + min(abs((int)y - 3), abs((int)y - 4))
          + min(abs((int)z - 3), abs((int)z - 4));
    return d == r || d == r + 1;
  }

  if (animation == 21) {
    int d = abs((int)x - 3) + abs((int)y - 3) + abs((int)z - 3);
    return ((d + f) % 4) < 2;
  }

  if (animation == 22) return ((x + y + z + f) % 8) == 0;

  if (animation == 23) {
    if (!((x == 0 || x == 7) && (y == 0 || y == 7) && (z == 0 || z == 7))) return false;
    uint8_t c = ((z == 7) ? 4 : 0) + ((y == 7) ? 2 : 0) + ((x == 7) ? 1 : 0);
    return c == (f % 8);
  }

  if (animation == 24) return firecrackerVoxel(f, x, y, z);
  if (animation == 25) return snakeVoxel(f, x, y, z);
  if (animation == 26) return rotatingHeartVoxel(f, x, y, z);

  return false;
}

} // namespace V2BuiltInAnimations
