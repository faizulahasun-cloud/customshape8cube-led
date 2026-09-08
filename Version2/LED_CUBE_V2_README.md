# LED CUBE — VERSION 2

## Physical LED numbering contract

- LED001 is the bottom-layer LED at Column 1.
- Columns 1–8 are the FRONT face.
- Numbering proceeds through the physical cube from this starting point.
- V2 keeps each physical LED as an individual definition in `LEDs/`.
- Frame files combine LED definitions; `+` means visual OR/combination, not arithmetic addition.
- Animation files sequence frames.
- Multiplexing remains a separate display-engine layer.

## Required V2 structure

```text
Version2/
├── Main.ino
├── LEDs/
│   ├── LED001.h ... LED512.h
├── Frames/
│   ├── Frame001.h ...
├── Animations/
│   ├── Animation001.h ...
└── Multiplexing/
    └── DisplayEngine.h
```
             BLUETOOTH
                 │
                 ▼
        Animation Selector
                 │
                 ▼
          Animation 01
                 │
       ┌─────────┼─────────┐
       ▼         ▼         ▼
    Frame 1   Frame 2   Frame 3 ...
       │         │         │
       ▼         ▼         ▼
  LED .h files combined for each frame
                 │
                 ▼
          Current 512-LED Frame
                 │
                 ▼
        EXISTING MULTIPLEXING
                 │
       ┌─────────┼─────────┐
       ▼         ▼         ▼
    Layer 1   Layer 2 ... Layer 8
                 │
                 ▼
          Shift Registers
                 │
                 ▼
             8×8×8 Cube

LED_CUBE/
│
├── Main.ino
│
├── LEDs/
│   ├── LED001.h
│   ├── LED002.h
│   ├── LED003.h
│   ├── ...
│   └── LED512.h
│
├── Frames/
│   ├── Frame001.h
│   ├── Frame002.h
│   ├── Frame003.h
│   └── ...
│
├── Animations/
│   ├── Animation001.h
│   ├── Animation002.h
│   └── ...
│
└── Multiplexing/
    └── DisplayEngine.h

LED001.h ─┐
LED002.h ─┤
LED075.h ─┼──→ Frame001.h
LED079.h ─┤
LED270.h ─┤
LED402.h ─┘
                ↓
          Animation001.h
                ↓
        Multiplexing engine
                ↓
             Cube
Follow this pattern and start creat step by step. If any confusion ask me. Before every step get confirmation from me. Got it? For pins follow our current files. Don’t delete them .

Existing files outside `Version2/` are not modified or deleted.