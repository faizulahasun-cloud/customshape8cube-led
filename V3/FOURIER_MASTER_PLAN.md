# V3 Fourier Custom-Function Firmware Master Plan

## Baseline

Repository: `faizulahasun-cloud/customshape8cube-led`

Base branch: `main`

Base commit locked for this version: `ca7f1f884feebb3e71325c87cbfbc11ff5619400`

New development branch: `v3-fourier-parser`

Current V3 parser source:
- `V3/V3FunctionConversion.h`

Current V3 firmware source:
- `V3/gifinal_unique_fixed.ino`

## Non-negotiable scope

This version changes the custom-function representation **only at the parser/compiler boundary**.

The following must remain untouched unless a later implementation step proves a compile/interface defect that cannot be solved inside the parser boundary:

- AnimationEngine / animation scheduling
- FrameEngine
- `frameData[64]` interface
- `voxelBuffer[2][8][8][8]`
- double buffering
- `commitFrame()`
- display-buffer generation
- Timer2 ISR and refresh timing
- layer multiplexing
- brightness handling
- built-in animations
- touch control
- BLE transport/chunking/queueing
- existing mode protocol (`A/M/N/C/R/@/E`)
- root `index.html`

The downstream renderer must continue to receive the same boolean voxel result through the existing:

`V3FunctionConversion::evaluate(X,Y,Z,F)`

interface.

## Design goal

Add a Fourier-series representation for custom functions so that smooth/periodic functions can be represented by a small coefficient set instead of a large general-purpose bytecode expression.

The parser/compiler chooses the representation:

`source expression -> parse -> analyze -> Fourier representation OR existing bytecode representation`

The rendering path remains:

`evaluate(X,Y,Z,F) -> existing drawAnimationFrame() -> existing frame buffers`

## Fourier model

The first implementation uses a finite real Fourier series over the animation phase/frame domain:

`g(F) = a0 + sum[k=1..H] (ak*cos(2*pi*k*F/N) + bk*sin(2*pi*k*F/N))`

where:

- `F = 0..49`
- `N = 50`
- `H` is the selected harmonic count
- coefficients are generated on the host/web/compiler side or derived during compilation
- Arduino runtime uses compact coefficients
- output is quantized to the existing boolean/voxel decision without changing downstream rendering

Spatial variables `X,Y,Z` remain part of the expression language. Fourier optimization is applied only when the expression can be safely transformed into the supported Fourier form. Unsupported expressions continue through the existing bytecode path.

## Representation strategy

### Fourier record

Use a compact fixed-point representation suitable for ATmega328P:

- DC coefficient
- signed cosine coefficients
- signed sine coefficients
- harmonic count
- phase/frame-domain metadata
- threshold/output metadata where required

Avoid storing `float` coefficients in the Arduino runtime representation unless a measured compile-size/accuracy test demonstrates a need.

Initial target precision:

- signed Q-format coefficients
- 16-bit coefficient storage
- deterministic integer arithmetic where practical
- no runtime FFT on the Uno

### Trigonometry

Do not call floating-point `sin()`/`cos()` for every voxel evaluation.

Use a parser-owned Fourier evaluator based on:

1. phase-indexed sine/cosine LUT, or
2. phase recurrence, or
3. a combination of LUT + symmetry.

The implementation must benchmark these choices before selecting the final one.

A compact AVR-compatible LUT is preferred when it provides the required accuracy without materially increasing flash usage.

## Compiler decision

The compiler must never silently change the mathematical meaning of an unsupported expression.

Decision order:

1. Validate source syntax.
2. Build/inspect the expression representation.
3. Determine whether it matches the supported Fourier transform form.
4. If safe, compile to Fourier coefficients.
5. Otherwise compile to the existing bytecode representation.
6. Reject only when neither representation can safely represent the expression.

## Required Fourier-supported forms

Phase 1 supports expressions that can be reduced to a finite Fourier sum in `F`.

Examples of intended targets:

- `A + B*sin(...F...)`
- `A + B*cos(...F...)`
- sums of harmonics
- equivalent constant/phase-shifted forms that can be normalized
- periodic smooth custom animation functions whose sampled output can be represented within the configured error tolerance

Do not claim arbitrary mathematical expressions are Fourier-optimized.

## Quantization and error policy

Because the cube has only 8 coordinate levels and the animation has only 50 frames, exact floating-point reconstruction is unnecessary.

The compiler must calculate reconstruction error against the source/reference samples.

Required checks:

- maximum absolute error
- RMS error
- threshold/voxel classification mismatch count
- maximum coefficient count
- coefficient overflow

A Fourier representation is accepted only when its reconstructed voxel decision matches the reference for all required test points, unless an explicit approximation mode is later added.

## Memory budget

The current V3 parser uses large static buffers. The Fourier implementation must not increase SRAM enough to destabilize the existing firmware.

Track separately:

- source buffer SRAM
- Fourier coefficient SRAM
- bytecode SRAM
- evaluation stack SRAM
- existing firmware SRAM
- free SRAM after compile

No renderer/display buffer may be resized.

## Execution phases

### Phase 0 — Baseline protection

- Verify the branch starts exactly at the locked main commit.
- Record current V3 file hashes.
- Compile the untouched baseline.
- Record flash/SRAM usage.
- Record existing custom-function test results.

Exit condition: baseline reproducible.

### Phase 1 — Fourier data model

Modify only the parser/compiler header.

Implement:

- Fourier structure
- coefficient storage
- harmonic metadata
- representation selector
- reset/state handling

No renderer changes.

Exit condition: header compiles with the existing firmware interface.

### Phase 2 — Fourier expression recognition

Extend the parser to identify supported Fourier forms.

Requirements:

- preserve existing grammar
- preserve X/Y/Z/F variables
- preserve existing bytecode fallback
- reject ambiguous transformations
- no changes to animation/rendering code

Exit condition: valid Fourier expressions compile to Fourier representation; ordinary expressions still compile to bytecode.

### Phase 3 — Coefficient generation

Implement coefficient generation/normalization.

Preferred path:

- host/compiler-side calculation where possible
- quantized coefficient packet stored on Arduino
- no runtime FFT on ATmega328P

For sampled arbitrary periodic functions, coefficient extraction may be performed by the web/phone compiler rather than the Arduino.

Exit condition: generated coefficients reconstruct reference samples within the configured tolerance.

### Phase 4 — AVR Fourier evaluator

Implement fixed-point evaluation inside the parser module.

Requirements:

- same `evaluate(X,Y,Z,F)` public interface
- no changes to `drawAnimationFrame()`
- no changes to `voxelBuffer`
- no changes to double buffering
- no changes to Timer2
- no floating-point trig in the inner evaluation path

Exit condition: Fourier and bytecode representations produce identical boolean results for the validation suite.

### Phase 5 — Runtime performance benchmark

Measure:

- evaluation time per `evaluate()`
- time for one complete 8x8x8 frame
- frame-generation time at the existing 200 ms frame period
- flash usage
- SRAM usage
- Fourier harmonic count versus runtime

Compare:

- existing bytecode
- Fourier 1 harmonic
- Fourier 2 harmonics
- Fourier 4 harmonics
- Fourier 8 harmonics

Exit condition: measured data identifies the practical harmonic limit for Uno.

### Phase 6 — BLE compatibility verification

Do not redesign BLE.

Verify that the existing:

`@ expression E`

and:

`R`

protocol still works.

The only changed behavior is the internal compiled representation.

Exit condition: the existing web controller protocol remains compatible.

### Phase 7 — Regression suite

Test existing expressions:

- constants
- X/Y/Z/F
- arithmetic
- modulo
- comparisons
- logical operators
- unary operators
- sin/cos
- sqrt/abs
- long expressions near current limits
- invalid expressions
- divide/modulo by zero
- stack overflow cases
- unsupported Fourier forms

Test Fourier expressions:

- DC only
- one harmonic
- multiple harmonics
- sine
- cosine
- phase shifted sine/cosine
- mixed harmonics
- boundary phases
- F=0
- F=49
- wrap from F=49 to F=0

Exit condition: no regression in the existing parser behavior and no renderer changes.

### Phase 8 — Physical cube validation

Flash the resulting firmware to the Uno.

Verify:

1. boot
2. built-in animations
3. Auto
4. Manual
5. Next
6. touch
7. brightness
8. BLE connection
9. custom-function transfer
10. compile confirmation
11. Fourier custom animation
12. return to built-in animation

The display pipeline must remain source-identical unless a compile/interface issue proves otherwise.

### Phase 9 — Final source audit

Verify changed-file list.

Expected implementation scope:

- `V3/V3FunctionConversion.h` or a parser-only Fourier companion included by it
- optional parser test/documentation files

Explicitly reject changes to:

- `V3/gifinal_unique_fixed.ino` display/rendering code
- root `index.html`
- built-in animation code
- buffer structures
- Timer2/multiplexing code

### Phase 10 — Release candidate

Create a release report containing:

- exact base commit
- changed files
- Fourier representation specification
- supported expression grammar
- harmonic limits
- coefficient precision
- flash/SRAM measurements
- evaluation timing
- reconstruction error
- regression results
- physical test results

Only after all gates pass should the branch become a merge candidate.

## Hard implementation rule

The parser module is an adapter between mathematical expressions and the existing animation renderer.

The renderer must remain mathematically unaware of Fourier.

The renderer receives the same `true/false` voxel decision it received before.

## Success criterion

A successful V3 Fourier version is not one that merely compiles.

It must demonstrate:

1. smaller/equivalent custom-function representation for suitable functions;
2. lower evaluation cost for supported Fourier functions on ATmega328P;
3. no loss of required 8x8x8 animation behavior;
4. no changes to the existing rendering/double-buffer architecture;
5. no regression of existing bytecode expressions;
6. exact or explicitly measured bounded output error.

## Current status

- Branch created from the current `main` commit.
- Master plan established.
- No changes have been made to `main`.
- No animation/rendering/double-buffer/Timer2 code is to be modified by this project.
