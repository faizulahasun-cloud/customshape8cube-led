# LED CUBE — VERSION 2

## Physical LED numbering contract

- LED001 is the bottom-layer LED at Column 1.
- Columns 1–8 are the FRONT face.
- Numbering proceeds through the physical cube from this starting point.
- LED number = Z*64 + Y*8 + X + 1
- V2 keeps each physical LED as an individual definition in `LEDs/`.
- Frame files combine LED definitions; `+` means visual OR/combination, not arithmetic addition.
- Animation engine will get animation from function conversion engine. then convert that data according to led definition files and send led position data to frame engine serially 1 by 1.
-then frame engine according to led position data follow the led files to create the frames 1 after another according to requirements. then send frame data to display engine
- `Main.ino` will responsible for send data from one engine to next engine according to logic.
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
- The current V2 engines have the basic interfaces needed for this pipeline, but the complete end-to-end connection/orchestration is implemented by `Main.ino` 



## Development rule

Build and modify Version 2 step by step. Before each step, obtain confirmation. If anything is unclear, ask before making changes. Follow the pin definitions already present in the current files. Do not delete existing files unless explicitly requested.

Existing files outside `Version2/` are not modified or deleted.
