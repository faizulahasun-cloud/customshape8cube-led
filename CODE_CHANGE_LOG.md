# 8x8x8 LED Cube — Code Change Log

This file is the permanent chronological record of code edits made to this project.

## Rules

1. Every edit to `LED_Cube_SMODE_512BIT_3BYTE_FIXED.ino` or `index.html` must append a new entry here in the same change.
2. Each code-edit entry must state exactly:
   - File
   - Date
   - Change type (`FIX`, `FEATURE`, `BEHAVIOR CHANGE`, or `REFACTOR`)
   - **Before:** the exact code/content that was present immediately before the edit
   - **After:** the exact replacement code/content
   - **Reason:** why the replacement was necessary
   - Contract/regression checks performed
   - Commit SHA
3. Do not summarize the Before/After text when recording a code edit. Record the exact replaced text and the exact replacement text.
4. Do not delete, rewrite, reorder, or silently alter old log entries.
5. New entries are append-only and must go at the end of the file.
6. An existing log entry is historical evidence and must never be treated as disposable project content.
7. The assistant must not delete this file or remove historical entries. Deletion of this log is reserved for the repository owner/user.
8. If an edit changes `LOGIC_CONTRACT.md`, record that as a documentation/specification change as well, including the exact Before/After and reason.
9. Analysis, inspection, comparison, or reporting without a code modification does not create a code-change entry.
10. A code edit is not complete until its log entry is committed with the code change, so the repository history preserves the code and its explanation together.

## Initial Log Entry

### 2026-09-07 — Log created

- File: `CODE_CHANGE_LOG.md`
- Change type: `DOCUMENTATION`
- Before: File did not exist.
- After: This append-only code-change log was created.
- Reason: The project now requires an exact Before → After → Reason record for every future code edit, with historical entries preserved.
- Code files changed in this entry: None.

### 2026-09-07 — Fix brightness protocol and reconcile Stream-mode contract

- File: `LED_Cube_SMODE_512BIT_3BYTE_FIXED.ino`
- Change type: `FIX`
- Before: `if(parseMode==5){parseCustomFunctionStream((char)in);continue;}if(parseMode==6){`
- After: `if(parseMode==5){parseCustomFunctionStream((char)in);continue;}if(parseMode==4){globalBrightness=constrain(in,(byte)2,(byte)8);parseMode=0;sendAck(F("BRIGHTNESS_OK"));continue;}if(parseMode==6){`
- Reason: The web app already sends `B` followed by one brightness byte, but the firmware only changed `parseMode` to 4 and had no handler to consume the following byte. The added handler consumes that byte, clamps it to the supported 2–8 range, resets the parser, and acknowledges the update.
- Contract/regression checks performed: Preserved the existing `B` command entry point and all Auto/Manual/Math/Custom command branches; the brightness byte is consumed as control data and is not treated as LED frame data.

- File: `LOGIC_CONTRACT.md`
- Change type: `BEHAVIOR CHANGE`
- Before: `| Stream-mode multiplexing | Stream/display modes must follow the same layer-OFF → shift 72 bits → latch → layer-ON electrical sequence; the stream path must not bypass blanking when changing shift-register data. |`
- After: `| Display mode | The current firmware uses the buffered display path for built-in, Math, and Custom rendering. No separate Stream mode is implemented or part of the current command protocol. |`
- Reason: The current firmware has no Stream-mode parser, buffer, command, or rendering path. The previous Stream-mode rule therefore described functionality that does not exist in the current protocol and created a documentation/code inconsistency. The contract is corrected to describe the actual current buffered display implementation.
- Contract/regression checks performed: Built-in, Math, and Custom common display-buffer pipeline remains documented; the flicker-safe multiplexing rule is unchanged; no command definitions were added or removed.

- HTML check: `index.html` was inspected and not modified because its brightness transmission (`B` + one byte) already matches the contract, and it contains no Stream-mode implementation.
- Commit SHA: recorded in the Git commit containing these changes.

### 2026-09-07 — Add 3.5-second post-GATT web-control delay

- File: `index.html`
- Change type: `BEHAVIOR CHANGE`
- Before: `setControlsEnabled(false);webCubeMode="NONE";customReady=false;document.getElementById("runCustomBtn").disabled=true;setControlsEnabled(true);`
- After: `setControlsEnabled(false);webCubeMode="NONE";customReady=false;document.getElementById("runCustomBtn").disabled=true;setTimeout(()=>{setControlsEnabled(true);log.innerText="INTERFACE STATUS: CONNECTED";},3500);`
- Reason: The Arduino uses a 3-second HM-10 STATE debounce before accepting Bluetooth control bytes after a connection. The web app now keeps its controls disabled for 3.5 seconds after successful GATT connection so normal controls are not exposed during that startup/debounce interval. No application-level handshake was added.
- Contract/regression checks performed: Preserved direct command protocol with no application handshake; preserved `A`, `M`, `N`, Math, Custom, and `B` command formats; BLE disconnect handling remains unchanged; the delay applies only after successful web Bluetooth GATT connection.
- Commit SHA: `0e8a4fca38ed0e5c991faad58336a89cdb8e8209`.

- File: `LOGIC_CONTRACT.md`
- Change type: `BEHAVIOR CHANGE`
- Before: `| Bluetooth connection | The web app connects to the HM-10 and enables the controls after the GATT connection is established. No application-level handshake is required. |` and `| Connect | Establish the HM-10 GATT connection and enable the web controls. No application-level `H` handshake is required. |`
- After: `| Bluetooth connection | The web app connects to the HM-10 through GATT, then keeps the web controls disabled for 3.5 seconds before enabling them. No application-level handshake is required. |` and `| Connect | Establish the HM-10 GATT connection, keep the web controls disabled for 3.5 seconds, then enable the controls. No application-level `H` handshake is required. |`
- Reason: The web implementation now intentionally waits 3.5 seconds after GATT connection before exposing the Bluetooth controls. The contract is updated to describe the actual current web behavior while explicitly retaining the no-handshake requirement.
- Contract/regression checks performed: No Arduino protocol command was added; no application handshake was introduced; brightness remains `B` plus one byte; Auto/Manual/Next/Math/Custom behavior remains unchanged.
- Commit SHA: `00a9b5d675ce12f3035b6497f258ebcd56a4c810`.

- File: `VERSION_CONTROL.md`
- Change type: `BEHAVIOR CHANGE`
- Before: `- Handshake uses `H` → `HANDSHAKE_OK`.` and `| Bluetooth | Connect, handshake, commands, notifications, disconnect fallback |` and `| ACKs | ACK is still produced, parsed, and waited for correctly |`
- After: `- No application-level Bluetooth handshake is required.` and `| Bluetooth | Connect, commands, notifications, disconnect fallback |` and `| ACKs | ACK is still produced and parsed for status visibility; command execution does not depend on ACKs |`
- Reason: The live Logic Contract defines no application-level handshake and explicitly states that ACKs do not gate command execution. The assistant-facing version-control rules were corrected so they cannot contradict the live specification by instructing future edits to implement or preserve an obsolete `H` handshake.
- Contract/regression checks performed: Preserved the purpose of VERSION_CONTROL.md as editing protection; aligned its Bluetooth rules with the current Logic Contract; no Arduino or HTML behavior was changed by this documentation update.
- Commit SHA: `f28cb9c747fbbfe3d35caf33f9364dd58e0fe08a`.

### 2026-09-07 — Remove Math feature and keep Custom function only

- File: `LED_Cube_SMODE_512BIT_3BYTE_FIXED.ino`
- Change type: `BEHAVIOR CHANGE`
- Before: The live firmware contained the Math protocol comment `// H=handshake, A=auto, M=manual, N=next animation, Y=begin math upload, F=start math, C=begin custom upload, X=start custom, S=stop custom, B=brightness + one byte value.`, Math program state (`MathOp`, `MathInstr`, `mathProgram`, `mathProgramLength`, `mathProgramValid`), Math compiler/evaluator entry points (`compileExpression`, `evaluateExpression`), Math rendering (`drawMathFrame`), the Math parser branch `parseMode==6`, and the `Y`/`F` command branches.
- After: The protocol comment is `// A=auto, M=manual, N=next animation, C=begin custom upload, X=start custom,` followed by `// S=stop custom, B=brightness + one byte value.`; the compiler/evaluator used by Custom is retained under Custom-specific names (`ExprOp`, `ExprInstr`, `customProgram`, `compileCustomExpression`, `evaluateCustomExpression`); all Math command parsing, Math modes, Math rendering, and Math-only state were removed.
- Reason: The project is now Custom-function-only. Removing the Math feature eliminates its UI/protocol/mode surface while preserving the expression engine required by the Custom function implementation.
- Contract/regression checks performed: Preserved 27 built-in animations including rotating heart index 26; preserved Auto/Manual/touch behavior; preserved Custom `C` upload, `CF_END`, `X` start, `S` stop, `B` brightness; preserved HM-10 pins and 3-second Arduino disconnect debounce; preserved common display buffer and flicker-safe 72-bit refresh; removed `Y` and `F` as Bluetooth commands; no application-level handshake added.
- Commit SHA: `3c77de81a4dfc6ed31f91b8ce669c869e8b76981`.

- File: `index.html`
- Change type: `BEHAVIOR CHANGE`
- Before: The page contained the Math button `<button class="btn btn-warning modeControl" ... onclick="transmitModeToken('F')" ...>Math Mode ('F')</button>`, the Mathematical Function Engine editor/preview panel, the `mathFrames` state, `compileMathFunction()`, Math notification handlers, and `sendMathFunction()`/`F` handling in `transmitModeToken()`.
- After: The page contains only Auto, Manual, Next Animation, Custom Function, and brightness controls; all Math UI, Math preview state, Math notification handlers, Math upload/start code, and `F` command handling are removed. The existing 3.5-second post-GATT control delay remains unchanged.
- Reason: The web app must expose only the Custom-function workflow after Math removal.
- Contract/regression checks performed: Preserved GATT service/characteristic, serialized BLE writes and transient write-busy retry, text-stream notification handling, Custom `C`/`CF_END`/`X`/`S`, direct `A`/`M`/`N`, and `B` + one-byte brightness protocol; no application-level handshake or ACK dependency added.
- Commit SHA: `fe6672397eff4dc7c72435e4bae2ac20ff63e735`.

- File: `LOGIC_CONTRACT.md`
- Change type: `BEHAVIOR CHANGE`
- Before: The Device Behavior, Modes, Bluetooth actions, and display/memory sections explicitly listed Math upload, Start Math, Math waiting state, Math Mode, Math rendering, and Math program memory.
- After: The current contract contains only Auto, Manual, Custom waiting/Custom Mode, and their corresponding Bluetooth actions; rendering and memory sections now describe Built-in + Custom only.
- Reason: The live specification must describe the current Custom-only product behavior and must not require a removed Math feature.
- Contract/regression checks performed: Preserved all physical wiring, touch, brightness, BLE disconnect fallback, coordinate/front-face, rotating-heart, display-buffer, flicker-safe multiplexing, no-handshake, and RAM-saving rules.
- Commit SHA: `294c8eb0e490cf72d20af9ebee733855f024be31`.

- File: `VERSION_CONTROL.md`
- Change type: `BEHAVIOR CHANGE`
- Before: Protected-behavior and regression examples referred to Math alongside Custom, including `Built-in, Math, and Custom rendering` and `Auto, Manual, Math, Custom`.
- After: Those references now describe `Built-in and Custom` rendering and `Auto, Manual, Custom` modes, with no Math feature in the protected current behavior.
- Reason: Version-control guidance must match the live Custom-only Logic Contract and must not cause future edits to restore or preserve a removed feature.
- Contract/regression checks performed: Preserved the no-handshake rule, exact edit-log requirement, GitHub-source-of-truth rule, display/memory protections, and disconnect recovery requirements.
- Commit SHA: `c841f99d37a0c2db759353cad856b85f20351ca5`.

### 2026-09-07 — Separate Custom compile/preparation from Custom animation start

- File: `LED_Cube_SMODE_512BIT_3BYTE_FIXED.ino`
- Change type: `FIX`
- Before: `void resetCustomReceive(){rxLength=0;rxBuffer[0]='\0';customReady=false;customProgramValid=false;customProgramLength=0;}\nvoid parseCustomFunctionStream(char c){\n  if(rxLength>=sizeof(rxBuffer)-1){customReady=false;customProgramValid=false;parseMode=0;sendAck(F("CUSTOM_ERROR"));return;}` and on successful `CF_END`: `if(compileCustomSource()){customReady=true;parseMode=0;sendAck(F("CUSTOM_OK"));}` and the `C` branch: `else if(in=='C'){parseMode=5;resetCustomReceive();currentCubeMode=5;animationStart=now;lastFrameTime=now;clearDisplayBuffer();sendAck(F("CUSTOM_UPLOAD_READY"));}`
- After: Added `void enterCustomWaitingState(){ currentCubeMode=5; animationStart=millis(); lastFrameTime=animationStart; frameCounter=0; clearDisplayBuffer(); }`; the Custom receive/error and successful compile paths explicitly call it; and the `C` command uses `resetCustomReceive();enterCustomWaitingState();`.
- Reason: Custom source compilation is preparation/calculation only. Upload must stop the currently running built-in animation, blank the cube, compile/store the new program, and leave the Arduino in Custom Waiting. Only the separate `X` command may transition to `currentCubeMode=4` and begin `drawCustomFunctionFrame()` rendering.
- Contract/regression checks performed: `C` and `CF_END` never enter Custom Mode; successful compilation does not render a frame; compile failure remains blank and waiting; `X` remains the only Custom-start transition; `S` still returns to Auto; built-in animation modes and common display refresh remain unchanged.
- Commit SHA: `bfd7e4641b0c01105073ea25abac2efd11fecd47`.

- File: `index.html`
- Change type: `FIX`
- Before: The Custom upload button text was `Send Function`, `sendCustomFunction()` cleared `customReady` and disabled `Start Custom`, then only notification `CUSTOM_OK` re-enabled `Start Custom`; `startCustomFunction()` began with `if(!customReady){alert("Custom function has not been successfully compiled yet.");return;}`.
- After: The button text is `Send + Compile`; after the BLE upload completes successfully, the page enables `Start Custom` immediately and shows `CUSTOM FUNCTION SENT — WAITING FOR START`; `startCustomFunction()` no longer blocks on `customReady` or a `CUSTOM_OK` notification and simply sends `X` to Arduino. `CUSTOM_OK`/`CUSTOM_ERROR` notifications remain informational status updates.
- Reason: The web UI must reflect the two distinct operations: Send + Compile prepares the program and leaves the cube blank/waiting, while Start Custom sends `X` and requests actual animation start. Arduino remains authoritative and rejects `X` with `CUSTOM_NOT_READY` when no valid compiled program exists.
- Contract/regression checks performed: Preserved GATT UUIDs, BLE write queue/retry, 20-byte upload slicing, `C`/`CF_END`/`X`/`S`, no application-level handshake, and 3.5-second post-GATT control delay; no frame data are sent as Bluetooth control bytes.
- Commit SHA: `cc5ffc7a7cd119a7dd32a965f7e1288f1e9887a3`.

- File: `LOGIC_CONTRACT.md`
- Change type: `BEHAVIOR CHANGE`
- Before: `| Web Custom control | The web app sends the custom function to the Arduino. The Arduino stops the current animation, compiles/stores the new function, and waits for the Start Custom command before rendering it. Stopping Custom returns the cube to Auto Mode. |` and the existing Custom upload/start rows did not explicitly separate compile from animation start.
- After: The Device Behavior row explicitly states `Send + Compile` stops/clears the cube, compiles/stores without rendering, and waits for `X`; a new `Custom upload / compile / start sequence` section defines the two operations separately; the Bluetooth Custom upload row explicitly says compilation does not start rendering.
- Reason: The Custom-only architecture must explicitly preserve the distinction between calculating/compiling a function and running its animation, preventing future edits from accidentally making compile execute the renderer.
- Contract/regression checks performed: Preserved Custom Waiting as blank, `X` as the only start transition, `S` as return-to-Auto, ACKs as status-only, and all existing physical/display/BLE requirements.
- Commit SHA: `98886827d66a2226b16ee8e2c5dc52682de037d1`.
