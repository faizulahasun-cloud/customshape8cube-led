# 8×8×8 Cube Function Test Library

These files are copy/paste test cases for the browser Custom Function editor and Math Function editor.

## Custom Function files
- `custom/01_diagonal_wave.txt` — moving diagonal wave
- `custom/02_center_pulse.txt` — pulsing center sphere
- `custom/03_layer_ripple.txt` — expanding layer/ring pattern
- `custom/04_tilted_plane.txt` — moving tilted plane
- `custom/05_two_point_orbit.txt` — two moving voxel points

## Mathematical function files
- `math/01_diagonal_wave.txt` — binary diagonal wave
- `math/02_sphere.txt` — animated sphere
- `math/03_checker_wave.txt` — animated checker wave
- `math/04_rotating_plane.txt` — moving plane
- `math/05_nested_wave.txt` — combined sine/cosine field

## Test method
1. Copy one file's contents into the corresponding editor in the web app.
2. Compile/send the function.
3. Start it with the normal Run/Start control.
4. The same mathematical meaning should be reproduced by the Arduino engine.

These are intentionally different patterns so they test arithmetic, variables, comparisons, boolean operators, and animation over `F`/`T` rather than only repeating the original built-in animation.
