# 8×8×8 LED Cube V3 — Professional Firmware Report

**Repository:** `faizulahasun-cloud/customshape8cube-led`  
**Branch audited:** `main`  
**Firmware:** `V3/gifinal_unique_fixed.ino`  
**Expression engine:** `V3/V3FunctionConversion.h`  
**Custom-function reference:** `V3/custom_functions_0-9.txt`  
**Report status:** Source-level engineering audit; no firmware files were modified by this report.

---

## 1. Executive Summary

The current V3 firmware is an Arduino Uno–class 8×8×8 multiplexed LED-cube controller with:

- 9 × 74HC595 shift-register display output.
- Double-buffered voxel generation and display data.
- Timer2-driven multiplex refresh.
- Hardware brightness control through an 8-level accumulator/PWM scheme.
- Built-in animation generation through coordinate/frame functions rather than stored 512-voxel frame tables.
- Auto and Manual operating modes.
- Physical touch and potentiometer control.
- HM-10 Bluetooth control using AltSoftSerial.
- A local custom-expression compiler and bytecode evaluator.
- Custom-program upload/wait/start separation.
- Non-blocking visual confirmation handling.
- Full display blanking during Custom Waiting.

The repository's `LOGIC_CONTRACT.md` is explicitly defined as the source of truth for intended device behavior, and it requires analysis/reporting to be separated from modification. fileciteturn6file0L2-L2

The principal engineering constraint remains **ATmega328P SRAM**. The Custom engine currently reserves a 192-character source buffer and 80-instruction bytecode buffer, while the evaluator also uses a 16-element `float` stack. The firmware therefore has a relatively tight RAM budget and should be treated as memory-sensitive production code. The current source was not compiled as part of this report, so this document does not claim a fresh Arduino IDE/compiler memory report.

**Overall assessment:** architecturally coherent V3 firmware with a sensible buffered display path and a properly separated Custom preparation/start workflow, but with a material SRAM-capacity risk that should be verified before declaring a final production release.

---

## 2. Audited Source Inventory

| Component | Current role | Audit result |
|---|---|---|
| `V3/gifinal_unique_fixed.ino` | Main Arduino firmware | Audited |
| `V3/V3FunctionConversion.h` | Custom source receiver, compiler and evaluator | Audited |
| `V3/custom_functions_0-9.txt` | Ten reference Custom digit expressions | Audited |
| `index.html` | Web Bluetooth controller | Audited for protocol/interface consistency |
| `LOGIC_CONTRACT.md` | Device behavior specification | Audited as behavioral authority |
| `CODE_CHANGE_LOG.md` | Historical code-change record | Audited as project history/protection |

The current `V3` directory contains the firmware, expression engine, and Custom 0–9 reference file; it does not contain a separate `Main.ino`. fileciteturn4file0L2-L2

The project README identifies the system as an HM-10 BLE-controlled 8×8×8 LED cube with Auto/Manual modes, Custom Functions, brightness control, and connection status. fileciteturn1file0L2-L2

---

## 3. Hardware and Pin Contract

Current firmware pin definitions:

| Function | Pin | Contract status |
|---|---:|---|
| 74HC595 DATA | D11 | Preserved |
| 74HC595 CLOCK | D13 | Preserved |
| 74HC595 LATCH | D12 | Preserved |
| Touchpad | D10 | Preserved |
| Brightness potentiometer | A0 | Preserved |
| HM-10 state | D2 | Required by contract; source-side state handling should be checked in the complete firmware path before hardware release |
| HM-10 serial | AltSoftSerial pins | Hardware-library assigned; not reassigned in the visible pin constants |

The Logic Contract explicitly protects D10, D11, D12, D13, D2, and A0 because the software, wiring, and hardware depend on these assignments. fileciteturn6file0L2-L2

No report-only action changes these assignments.

---

## 4. Display Architecture

### 4.1 Voxel representation

The firmware stores each `(X,Y)` column as one byte containing eight Z bits:

```text
voxelBuffer[buffer][X][Y] -> 8 Z bits
```

There are two voxel buffers, allowing one buffer to be generated while the other represents the active frame.

### 4.2 Display representation

The firmware separately maintains:

```text
displayBuffer[buffer][layer][register]
```

Each display buffer is 8 × 8 bytes = 64 bytes. Two buffers are maintained for atomic frame swapping.

### 4.3 Frame commit

`commitFrame()` swaps both voxel and display buffer roles inside an interrupt-disabled section. This provides a clear frame boundary and prevents the Timer2 ISR from observing a partially swapped buffer index.

### 4.4 Display preparation

`prepareDisplayData()` converts the generated voxel state into the physical 8-register column representation using `COLUMN_MAP`. This is a clean separation between logical cube coordinates and shift-register output ordering.

### 4.5 Physical mapping

The current column map is organized as 64 logical columns across eight registers, with each register exposing eight bits. The firmware therefore generates 64 column bytes plus one layer byte per multiplex slot: a total of **72 serial bits / 9 bytes**.

---

## 5. Flicker-Safe Multiplexing

The refresh ISR executes `refreshDisplay()` through Timer2.

The refresh sequence is conceptually:

1. Select the active display buffer.
2. Calculate whether the current layer is enabled from the brightness accumulator.
3. Shift the layer byte.
4. Shift all eight column-register bytes.
5. Latch the complete 72-bit state.
6. Advance to the next layer.

The project contract explicitly requires the electrical sequence to prevent ghosting: layer outputs must be off while the new 72-bit data is shifted and latched, followed by enabling only the selected layer. fileciteturn6file0L2-L2

The current source's `refreshDisplay()` shifts the layer byte followed by the eight column-register bytes and latches the result. `blankCubeAndStop()` additionally disables the refresh ISR, clears both voxel/display buffers, clears brightness accumulators, shifts nine zero bytes, and latches them.

**Engineering observation:** the source implements the required buffered display path and an explicit hard-blank operation. The report does not claim oscilloscope/logic-analyzer verification of the physical layer-off timing.

---

## 6. Timer2 Refresh Configuration

The firmware configures Timer2 in CTC mode:

```text
TCCR2A = WGM21
TCCR2B = CS22 | CS21 | CS20
OCR2A  = 3
TIMSK2 = OCIE2A
```

At the standard 16 MHz Arduino Uno clock, this configuration corresponds to a Timer2 compare interrupt period of approximately **256 µs**, or approximately **3906.25 ISR calls/second**.

With eight multiplex layers, this gives approximately **488.28 complete layer scans/second** before considering any additional software timing effects.

This is consistent with the project's protected high-rate multiplex refresh architecture.

---

## 7. Brightness Control

`globalBrightness` is stored as an 8-bit value and initialized to level 5.

The display ISR uses an accumulator for each layer:

```text
brightnessAccumulator[layer] += globalBrightness
```

A layer is enabled when the accumulator reaches the threshold of 8, after which the threshold is subtracted.

This creates fractional layer duty control without a separate slow software PWM loop.

The project contract defines brightness levels 2–8 for the user-facing control. fileciteturn6file0L2-L2

The physical potentiometer is intended to control brightness when Bluetooth is disconnected; the web interface is intended to control it while Bluetooth is connected. fileciteturn6file0L2-L2

---

## 8. Frame Generation Architecture

The firmware follows the preferred low-RAM architecture stated by the project contract:

```text
calculate current animation state
        ↓
build one frame
        ↓
copy/prepare display data
        ↓
commit buffer
        ↓
Timer2 refreshes display
```

The contract specifically prefers calculated animation state over large stored frame tables because of the ATmega328P RAM limitation. fileciteturn6file0L2-L2

The firmware contains function-based voxel generators for built-in effects and a unified `animationVoxel()` dispatcher. The visible source includes effects such as firecracker, snake, rotating heart, directional sweep, sphere, tunnel, helix, math-grid-style patterns, matrix rain, wire cube, plasma, curtain, and orbital-helix behavior.

The rotating heart is implemented as a normal voxel function and is routed through the built-in animation system, matching the contract requirement that it not be treated as a separate display mechanism. fileciteturn6file0L2-L2

---

## 9. Animation and Mode Model

The current firmware declares:

```text
TOTAL_ANIMATIONS = 38
BUILTIN_ANIMATIONS = 37
BLUETOOTH_FUNCTION_ANIMATION = 37
```

This establishes 37 built-in animation slots plus one runtime Custom-function animation slot.

### Auto Mode

Auto mode is intended to cycle through built-in animations using the configured carousel timing. The source declares a 10-second carousel interval.

### Manual Mode

Manual mode keeps the selected built-in animation active until the user or web interface advances it.

### Custom Waiting

The `C` workflow stops the previous animation and enters a blank Custom Waiting state. The source uses `blankCubeAndStop()` and confirmation/state logic rather than allowing the previous animation to continue running.

### Custom Mode

Custom rendering begins only after a valid program exists and the explicit start command is processed.

### Stop Custom

The project contract defines `S` as the Custom stop command and requires a return to built-in Auto Mode. fileciteturn6file0L2-L2

---

## 10. Physical Touch Behavior

The Logic Contract defines two distinct touch behaviors:

- Hold for more than 3 seconds: toggle Auto ↔ Manual while preserving the selected animation.
- Short touch: advance to the next built-in animation only while in Manual Mode.

When Bluetooth is connected, physical touch is intended to be ignored because the web interface is controlling the cube. fileciteturn6file0L2-L2

This report records the contract requirement; a fresh hardware timing test was not performed.

---

## 11. Bluetooth Architecture

The web interface uses the standard HM-10 UART-over-GATT pattern:

```text
Service:        FFE0
Characteristic: FFE1
```

The web application serializes BLE writes through a promise queue and sends payloads in **20-byte chunks** with a **20 ms inter-chunk delay**. This is explicitly visible in the current `index.html`. fileciteturn10file0L2-L2

The README likewise identifies HM-10 BLE as the wireless control mechanism. fileciteturn1file0L2-L2

The Logic Contract requires BLE write-busy conditions to be handled as transient conditions and requires command execution not to depend on ACK reception. fileciteturn6file0L2-L2

---

## 12. Bluetooth Command Model

The current contract defines the following control protocol:

| Command | Meaning |
|---|---|
| `A` | Built-in Auto Mode |
| `M` | Built-in Manual Mode |
| `N` | Next built-in animation |
| `C` | Enter Custom Waiting / begin Custom upload |
| `X` | Start Custom animation after valid program acceptance |
| `S` | Stop Custom and return to Auto |
| `B` + one byte | Set brightness |

Custom source transport uses reception delimiters in the current V3 web/firmware architecture, with `@` beginning reception and `E` ending reception. The current web page describes this exact mechanism and separately uses `R` to compile/start the stored function in the current implementation. fileciteturn10file0L2-L2

**Important contract consistency point:** the current `LOGIC_CONTRACT.md` describes the authoritative Custom workflow using `C`, source/`CF_END`, then `X`, while the current V3 `index.html` describes the live implementation as `C`, `@expressionE`, then `R`. This is a documentation/protocol discrepancy that should be reconciled before a production release. The report does not modify either side.

---

## 13. Custom Function Engine

`V3FunctionConversion.h` implements a local expression engine that:

1. Receives printable ASCII function characters.
2. Stores the source in RAM.
3. Parses the expression.
4. Emits compact bytecode instructions.
5. Evaluates the bytecode for each voxel coordinate and animation frame.

The engine supports variables:

```text
X, Y, Z, F
```

where X/Y/Z are 0–7 voxel coordinates and F is the animation frame.

Supported operators/functions include:

- Numeric constants.
- Addition, subtraction, multiplication, division and modulo.
- Unary negation.
- Logical NOT.
- `<`, `<=`, `>`, `>=`, `==`, `!=`.
- `&&`, `||`.
- `sin()`, `cos()`, `sqrt()`, `abs()`.

The parser explicitly rejects unsupported single-letter identifiers and reserved Bluetooth command characters rather than silently aliasing them. fileciteturn7file0L2-L2

---

## 14. Custom Engine Memory Profile

The current source declares:

```text
MAX_FUNCTION_LENGTH = 192
MAX_BYTECODE_LENGTH = 80
EVALUATOR_STACK_SIZE = 16
```

The important RAM allocations are approximately:

| Allocation | Approx. RAM |
|---|---:|
| `functionBuffer[193]` | 193 B |
| `bytecode[80]` (`uint8_t + float`) | ~400 B on AVR ABI |
| `voxelBuffer[2][8][8]` | 128 B |
| `displayBuffer[2][8][8]` | 128 B |
| brightness accumulators | 8 B |
| evaluator stack `float[16]` | 64 B during evaluation |
| Other globals/state | Additional RAM |

The bytecode representation is particularly important because each instruction contains an opcode plus a `float` value. Increasing bytecode capacity therefore has a direct and significant SRAM cost.

The source currently has **no separate instruction backup array**, which avoids duplicating the bytecode allocation.

**Production concern:** the ATmega328P has only 2 KB SRAM. A firmware build must therefore be checked for both static SRAM usage and runtime stack/heap collision before hardware release.

---

## 15. Custom Function Evaluation Safety

The evaluator contains explicit stack-bound checks before pushes and operand-count checks before binary/unary operations.

Division and modulo reject a zero divisor.

`sqrt()` clamps negative input to zero before evaluation.

An invalid opcode returns failure.

An invalid program therefore does not simply execute arbitrary memory; evaluation fails closed.

This is a positive safety property for a user-editable expression engine.

---

## 16. Custom Digit Reference Set

`V3/custom_functions_0-9.txt` defines ten separate digit expressions intended to occupy one Y-plane and travel from front to back:

```text
Y = F % 8
```

with:

```text
F=0 → Y=0 FRONT
F=7 → Y=7 BACK
F=8 → Y=0 FRONT again
```

The file explicitly defines `Y=0` as the physical front and `Y=7` as the back. fileciteturn9file0L2-L2

The reference file also records that Custom 9 was repaired and that its expression requires more bytecode capacity than the earlier parser configuration. fileciteturn9file0L2-L2

---

## 17. Confirmation State Machine

The firmware contains a non-blocking confirmation subsystem:

```text
CONFIRMATION_RECEIVED
CONFIRMATION_COMPILED
```

Each confirmation phase is driven by `millis()` rather than `delay()`.

The display is temporarily filled/cleared through the existing display buffer, while stored voxel data, the function source, compiled bytecode, and animation state are intentionally preserved.

The configured confirmation phase is 250 ms, making the sequence non-blocking and compatible with continued firmware servicing.

This is preferable to inserting blocking delays into the main loop of a multiplexed display controller.

---

## 18. Full-Cube Blank Behavior

`blankCubeAndStop()` is an explicit hard reset of the visible cube state:

1. Stop Timer2 refresh interrupts.
2. Clear both voxel buffers.
3. Clear both display buffers.
4. Reset brightness accumulators.
5. Shift eight zero column bytes plus one zero layer byte.
6. Latch the zero state.
7. Leave multiplexing stopped until another operation explicitly restarts it.

This is a strong implementation of the contract requirement that Custom Waiting be completely blank rather than displaying the previous animation. fileciteturn6file0L2-L2

---

## 19. Web Controller Consistency Review

The current web controller contains:

- HM-10 connection/disconnection controls.
- Auto.
- Manual.
- Next Animation.
- Custom Mood/Custom Waiting.
- Custom source editor.
- Custom source transmission.
- Custom run control.
- 20-byte BLE chunking.
- Serialized BLE writes.
- UI state reset on disconnect.

The current HTML explicitly limits source length to 192 characters and bytecode capacity to 80 instructions in its displayed documentation. fileciteturn10file0L2-L2

The README documents the same broad control model: Bluetooth, Auto/Manual, Custom Functions, brightness, and connection status. fileciteturn1file0L2-L2

---

## 20. Source-Level Verification Matrix

| Area | Verification performed | Result |
|---|---|---|
| Pin constants | Inspected current firmware | Pass at source level |
| 8×8 voxel buffers | Inspected declarations and frame flow | Pass |
| Double buffering | Inspected commit/swap logic | Pass |
| 72-bit display path | Inspected shift/latch path | Pass at source level |
| Timer2 CTC refresh | Inspected timer configuration | Pass at source level |
| Brightness accumulator | Inspected refresh implementation | Pass at source level |
| Full cube blanking | Inspected `blankCubeAndStop()` | Pass at source level |
| Custom source buffer | Inspected parser engine | Pass |
| Bytecode bounds | Inspected `emit()` and evaluator checks | Pass |
| Evaluator arithmetic guards | Inspected divide/modulo/stack checks | Pass |
| Custom digit coordinate convention | Inspected reference file | Pass |
| BLE 20-byte transport | Inspected web controller | Pass |
| BLE serialized writes | Inspected web controller | Pass |
| Custom protocol | Compared firmware architecture, HTML, and Logic Contract | **Documentation/protocol discrepancy found** |
| Arduino SRAM | Static allocation reviewed | **High-risk; fresh compile required** |
| Physical LED orientation | Source/contract reviewed | Hardware test not performed |
| Ghosting/flicker | Source sequence reviewed | Instrumented hardware test not performed |
| BLE RF reliability | Source reviewed | Live radio test not performed |
| Touch timing | Contract reviewed | Hardware timing test not performed |

---

## 21. Known Risks and Limitations

### Critical

**None conclusively established by this source-only audit.**

### High

1. **ATmega328P SRAM pressure.** The 192-byte-class source buffer, 400-byte-class bytecode allocation, two display/voxel buffers, Bluetooth/runtime state, and evaluator stack leave limited headroom. A fresh compile and runtime stack analysis are required before production release.
2. **Custom protocol specification mismatch.** `LOGIC_CONTRACT.md` describes the authoritative sequence using `CF_END` and `X`, while the current V3 web page documents `@expressionE` and `R`. These must describe the same live protocol before the system can be considered fully specification-locked.

### Medium

1. **No physical timing instrumentation in this report.** The multiplexing sequence is source-correct by inspection, but no oscilloscope/logic-analyzer verification was performed.
2. **No fresh Arduino build result is included.** This report intentionally avoids inventing a compiler memory figure.
3. **Custom expression evaluation uses floating point.** This increases execution cost compared with integer/fixed-point evaluation and may limit practical Custom expression complexity/frame rate on an ATmega328P.

### Low

1. The repository currently contains substantial historical documentation and change-log material; this is useful for traceability but increases documentation surface area.

---

## 22. Production Readiness Assessment

| Category | Assessment |
|---|---|
| Hardware pin discipline | Good |
| Display buffering | Good |
| Multiplex architecture | Good |
| Flicker/ghosting design intent | Good; physical verification pending |
| Brightness architecture | Good |
| Built-in animation architecture | Good / RAM-efficient |
| Custom compiler isolation | Good |
| Custom evaluator bounds checks | Good |
| BLE write serialization | Good |
| Custom waiting/blanking | Good |
| Protocol specification consistency | **Needs reconciliation** |
| SRAM margin | **Needs measured build verification** |
| Physical production validation | **Not completed by this report** |

**Release recommendation:** **Engineering candidate / not yet final production sign-off.**

The firmware architecture is sufficiently structured for continued development, but final sign-off should wait for a verified compile-memory report and reconciliation of the live Custom protocol against `LOGIC_CONTRACT.md`.

---

## 23. Change-Control Status

This report is documentation only. No firmware or web-controller behavior was changed while producing it.

The repository's change-log rules explicitly state that analysis, inspection, comparison, or reporting without code modification does not create a code-change entry. fileciteturn11file0L2-L2

Therefore this report does not alter historical firmware change records.

---

## 24. Final Engineering Conclusion

The current V3 firmware demonstrates a sound embedded architecture for an ATmega328P 8×8×8 LED cube:

- logical voxel generation is separated from physical display serialization;
- double buffering protects visible frame transitions;
- Timer2 provides deterministic high-rate multiplex servicing;
- brightness is integrated into the layer refresh path;
- Custom source compilation is isolated from LED-buffer management;
- the bytecode evaluator contains meaningful bounds and arithmetic checks;
- Custom Waiting has an explicit hard-blank implementation;
- BLE writes are serialized and chunked for the HM-10 transport;
- and the project maintains a formal device-behavior contract.

The two items requiring engineering attention before calling the firmware final are **SRAM margin** and **Custom protocol/contract reconciliation**. These are concrete, source-supported findings rather than speculative feature requests.

**Report conclusion: V3 is structurally sound but should remain under engineering validation until memory usage and the Custom command contract are formally verified against the exact release build.**
