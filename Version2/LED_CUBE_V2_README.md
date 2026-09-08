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

Existing files outside `Version2/` are not modified or deleted.