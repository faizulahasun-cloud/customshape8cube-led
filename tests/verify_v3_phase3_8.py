from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INO = ROOT / "V3" / "gifinal_unique_fixed.ino"
REG = ROOT / "V3" / "AnimationRegistry.h"

ino = INO.read_text(encoding="utf-8")
reg = REG.read_text(encoding="utf-8")

# Phase 3: IDs 0-36 remain built-in and are registered without per-animation
# frame storage.
for animation_id in range(37):
    assert f"{{{animation_id},  ANIMATION_FLAG_BUILTIN, animationVoxel}}" in reg or f"{{{animation_id}, ANIMATION_FLAG_BUILTIN, animationVoxel}}" in reg
assert "{37, ANIMATION_FLAG_CUSTOM,  animationVoxel}" in reg
assert "static_assert(ANIMATION_REGISTRY_COUNT == 38" in reg

# Phase 4: custom/Bluetooth animation uses the same generator entry point.
assert "bluetoothFunctionVoxel" in ino
assert "V3FunctionConversion::evaluate(x,y,z,f)" in ino

# Phase 5/6: one animation index, one frame path, and no animation-sized frame
# table introduced by the registry.
assert "drawAnimationFrame(animationIndex,frameCounter)" in ino
assert "prepareDisplayData();" in ino
assert "commitFrame();" in ino
assert "voxelBuffer[2][8][8]" in ino
assert "displayBuffer[2][8][8]" in ino

# Phase 7: protect the established display timing and buffer architecture.
assert "OCR2A=3" in ino
assert "ISR(TIMER2_COMPA_vect)" in ino
assert "activeDisplayBuffer" in ino
assert "noInterrupts();" in ino and "interrupts();" in ino

print("V3 Phase 3-8 static architecture checks: PASS")
