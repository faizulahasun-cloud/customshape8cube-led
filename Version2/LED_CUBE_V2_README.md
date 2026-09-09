# LED CUBE — VERSION 2

## Physical LED numbering contract

- LED001 is the bottom-layer LED at Column 1.
- Columns 1–8 are the FRONT face.
- Numbering proceeds through the physical cube from this starting point.
- LED number = Z*64 + Y*8 + X + 1
- V2 keeps each physical LED as an individual definition in `LEDs/`.
- Frame files combine LED definitions; `+` means visual OR/combination, not arithmetic addition.
- Animation structure/files are **not defined yet**. No `Animation001`, `Animation002`, etc. are assumed or created.
- `Main.ino` will be added later, after the engines are finalized.
- Existing files are not deleted unless explicitly requested.

## V2 data flow

```text
Bluetooth
   ↓
Function Character Bytes
   ↓
FunctionConversionEngine
   ↓
AnimationEngine
   ↓
LED positions / frame data
   ↓
FrameEngine
   ↓
DisplayEngine
   ↓
Multiplexing
   ↓
8×8×8 Cube
```

## Engine architecture

- Bluetooth sends function character bytes to `FunctionConversionEngine`.
- `FunctionConversionEngine` processes the received function data and passes the resulting information to `AnimationEngine`.
- `AnimationEngine` generates the required LED positions / frame data and passes it to `FrameEngine`.
- `FrameEngine` builds the current 512-LED frame and passes the display frame to `DisplayEngine`.
- `DisplayEngine` is the final hardware/display stage and handles the multiplexing of the cube.
- The engines are intended to pass their generated data sequentially rather than each engine independently implementing the whole system.
- The current V2 engines have the basic interfaces needed for this pipeline, but the complete end-to-end connection/orchestration is not implemented yet because `Main.ino` does not exist.

## V2 structure currently planned

```text
Version2/
├── Main.ino                 ← added later
├── LEDs/
│   └── LED001.h ... LED512.h
├── Frames/
│   ├── Frame001.h ...
│   └── FrameEngine.h
├── Animations/
│   └── animation structure to be decided later
├── FunctionConversion/
│   └── FunctionConversionEngine.h
└── Multiplexing/
    └── DisplayEngine.h
```

## Development rule

Build and modify Version 2 step by step. Before each step, obtain confirmation. If anything is unclear, ask before making changes. Follow the pin definitions already present in the current files. Do not delete existing files unless explicitly requested.

Existing files outside `Version2/` are not modified or deleted.
