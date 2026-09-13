#pragma once
#include <Arduino.h>
#include <avr/pgmspace.h>

// Phase 1: lightweight animation registry definition.
// The registry stores IDs/metadata and a reference to the existing frame
// generator only. It does not own voxel/display frame storage.
using AnimationVoxelGenerator = bool (*)(byte animationId, byte frame, byte x, byte y, byte z);

struct AnimationDescriptor {
  byte id;
  byte flags;
  AnimationVoxelGenerator generator;
};

constexpr byte ANIMATION_FLAG_BUILTIN = 0x01;
constexpr byte ANIMATION_FLAG_CUSTOM  = 0x02;

// Existing V3 frame generator. Phase 1 only registers the existing generator;
// visual algorithms remain untouched. Phase 2 will make this registry the
// single execution entry point.
bool animationVoxel(byte animationId, byte frame, byte x, byte y, byte z);

const AnimationDescriptor ANIMATION_REGISTRY[] PROGMEM = {
  {0,  ANIMATION_FLAG_BUILTIN, animationVoxel},
  {1,  ANIMATION_FLAG_BUILTIN, animationVoxel},
  {2,  ANIMATION_FLAG_BUILTIN, animationVoxel},
  {3,  ANIMATION_FLAG_BUILTIN, animationVoxel},
  {4,  ANIMATION_FLAG_BUILTIN, animationVoxel},
  {5,  ANIMATION_FLAG_BUILTIN, animationVoxel},
  {6,  ANIMATION_FLAG_BUILTIN, animationVoxel},
  {7,  ANIMATION_FLAG_BUILTIN, animationVoxel},
  {8,  ANIMATION_FLAG_BUILTIN, animationVoxel},
  {9,  ANIMATION_FLAG_BUILTIN, animationVoxel},
  {10, ANIMATION_FLAG_BUILTIN, animationVoxel},
  {11, ANIMATION_FLAG_BUILTIN, animationVoxel},
  {12, ANIMATION_FLAG_BUILTIN, animationVoxel},
  {13, ANIMATION_FLAG_BUILTIN, animationVoxel},
  {14, ANIMATION_FLAG_BUILTIN, animationVoxel},
  {15, ANIMATION_FLAG_BUILTIN, animationVoxel},
  {16, ANIMATION_FLAG_BUILTIN, animationVoxel},
  {17, ANIMATION_FLAG_BUILTIN, animationVoxel},
  {18, ANIMATION_FLAG_BUILTIN, animationVoxel},
  {19, ANIMATION_FLAG_BUILTIN, animationVoxel},
  {20, ANIMATION_FLAG_BUILTIN, animationVoxel},
  {21, ANIMATION_FLAG_BUILTIN, animationVoxel},
  {22, ANIMATION_FLAG_BUILTIN, animationVoxel},
  {23, ANIMATION_FLAG_BUILTIN, animationVoxel},
  {24, ANIMATION_FLAG_BUILTIN, animationVoxel},
  {25, ANIMATION_FLAG_BUILTIN, animationVoxel},
  {26, ANIMATION_FLAG_BUILTIN, animationVoxel},
  {27, ANIMATION_FLAG_BUILTIN, animationVoxel},
  {28, ANIMATION_FLAG_BUILTIN, animationVoxel},
  {29, ANIMATION_FLAG_BUILTIN, animationVoxel},
  {30, ANIMATION_FLAG_BUILTIN, animationVoxel},
  {31, ANIMATION_FLAG_BUILTIN, animationVoxel},
  {32, ANIMATION_FLAG_BUILTIN, animationVoxel},
  {33, ANIMATION_FLAG_BUILTIN, animationVoxel},
  {34, ANIMATION_FLAG_BUILTIN, animationVoxel},
  {35, ANIMATION_FLAG_BUILTIN, animationVoxel},
  {36, ANIMATION_FLAG_BUILTIN, animationVoxel},
  {37, ANIMATION_FLAG_CUSTOM,  animationVoxel}
};

constexpr byte ANIMATION_REGISTRY_COUNT = sizeof(ANIMATION_REGISTRY) / sizeof(ANIMATION_REGISTRY[0]);
static_assert(ANIMATION_REGISTRY_COUNT == 38, "Animation registry must preserve IDs 0-37");

inline bool animationRegistryContains(byte id) {
  return id < ANIMATION_REGISTRY_COUNT;
}

inline AnimationDescriptor animationRegistryGet(byte id) {
  AnimationDescriptor descriptor;
  memcpy_P(&descriptor, &ANIMATION_REGISTRY[id], sizeof(descriptor));
  return descriptor;
}
