# 8×8×8 Cube Function Test Library

These files are copy/paste test cases for the browser Custom Function editor.

## Custom Function engine

The cube receives **one custom program at a time**. The browser sends the program source once over BLE; the Arduino compiles it locally and then evaluates the compiled expression for all 512 voxels on every frame. It does **not** receive 512 bits for every frame.

Custom is geometry-neutral. There is no fixed list of shapes. A program can describe planes, curves, spheres, shells, particles, rotations, intersections, unions, waves, conditional motion, or combinations of them using the available expression operations.

Supported custom structure:
- `CUSTOM` / `END` delimit the program.
- `X`, `Y`, `Z`, `F` are the voxel coordinates and animation frame.
- Helper variables can be assigned with `NAME=expression` and may reference variables defined later.
- `IF condition OFF` keeps voxels where the condition is false.
- `IF condition ON` keeps voxels where the condition is true.
- `ON IF condition` and `OFF IF condition` are also accepted.
- A plain expression line is treated as an ON condition.
- `Z=expression` or multiple `Z=... OR Z=...` rules select allowed layers.
- Arithmetic: `+ - * / %`, comparisons, `&& || !`, and `sin`, `cos`, `sqrt`, `abs`.

The only limits are practical AVR resources (program/source size and helper-variable storage), not a predefined shape or animation list. Since only one function is active at a time, a new function replaces the previous custom program after it successfully compiles.

## Custom Function files
- `custom/01_diagonal_wave.txt` — moving diagonal wave
- `custom/02_center_pulse.txt` — pulsing center sphere
- `custom/03_layer_ripple.txt` — expanding layer/ring pattern
- `custom/04_tilted_plane.txt` — moving tilted plane
- `custom/05_two_point_orbit.txt` — two moving voxel points
- `custom/06_firecracker_upward.txt` — firecracker rises upward, then expands into a burst
- `custom/07_snake_random_direction.txt` — snake-like 3D wave with pseudo-random direction changes
- `custom/08_rotating_heart_mid_axis.txt` — heart rotating around the cube's center Z axis
- `custom/09_rotating_anchor_mid_axis.txt` — anchor rotating around the cube's center Y axis

## Test method
1. Copy one file into the **Custom Function** editor in `index.html`.
2. Press **Send + Compile**. This sends `C`, the source, and `CF_END`; Arduino stops the current animation, clears the cube, compiles/stores the function, and remains blank in Custom Waiting.
3. **Do not expect compilation to start the animation.** `CUSTOM_OK` / `CUSTOM_ERROR` are status acknowledgements only.
4. After the function has been sent, press **Start Custom**. This sends `X`; only then does Arduino begin Custom frame generation.
5. To stop Custom, press **Stop Custom**. Arduino returns to Auto Mode.
6. Only the selected function runs; functions are not queued or combined.
