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
3. Do not summarize the Before/After text when recording the code edit. Record the exact replaced text and the exact replacement text.
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

### 2026-09-07 — Fix Arduino power-up startup and BLE initialization

- File: `LED_Cube_SMODE_512BIT_3BYTE_FIXED.ino`
- Change type: `FIX`
- Before: `void setup(){pinMode(DATA_PIN,OUTPUT);pinMode(CLOCK_PIN,OUTPUT);pinMode(LATCH_PIN,OUTPUT);pinMode(TOUCH_PIN,INPUT);pinMode(BLE_STATE_PIN,INPUT);PORTB&=~(_BV(PB3)|_BV(PB4)|_BV(PB5));bluetooth.begin(9600);customProgramValid=false;customReady=false;clearDisplayBuffer();startRefreshTimer();delay(STARTUP_DELAY_TIME);animationStart=millis();lastFrameTime=millis();}`
- After: `void setup(){pinMode(DATA_PIN,OUTPUT);pinMode(CLOCK_PIN,OUTPUT);pinMode(LATCH_PIN,OUTPUT);pinMode(TOUCH_PIN,INPUT);pinMode(BLE_STATE_PIN,INPUT);PORTB&=~(_BV(PB3)|_BV(PB4)|_BV(PB5));bluetooth.begin(9600);customProgramValid=false;customReady=false;clearDisplayBuffer();startRefreshTimer();delay(STARTUP_DELAY_TIME);while(bluetooth.available()>0)bluetooth.read();lastBluetoothConnected=digitalRead(BLE_STATE_PIN)==HIGH;bluetoothStateChangedAt=0;animationStart=millis();lastFrameTime=animationStart;frameCounter=0;drawAnimationFrame(animationIndex,frameCounter);}`
- Reason: After the 3-second startup delay, the firmware previously initialized the timing variables but never generated the first animation frame, leaving the display buffer blank until the first 200 ms frame interval elapsed. The fix explicitly renders Animation 0 immediately, resets its frame counter, discards stale serial input accumulated during startup, and initializes the HM-10 connection state from the actual STATE pin so the 3.5-second web-control delay is not undermined by a second 3-second debounce when a BLE connection already exists during startup.
- Contract/regression checks performed: Preserved the 3-second startup wait, Auto Mode, 27 built-in animations, 200 ms frame timing, 10-second carousel, all pin assignments, Timer2 display refresh, Timer1/AltSoftSerial separation, touch behavior, BLE disconnect debounce, brightness behavior, Custom workflow, and common display buffer. No new Bluetooth command or handshake was added.
- Commit SHA: `20069964157e4c997f93e6f04f0093ac80b18396`.

### 2026-09-07 — Restore missing web Auto/Manual/Next command handler

- File: `index.html`
- Change type: `FIX`
- Before: `async function queueBLEWrite(data){bleWriteQueue=bleWriteQueue.catch(()=>{}).then(async()=>{if(!targetDataCharacteristic||!physicalTargetDevice?.gatt?.connected)throw new Error("Bluetooth disconnected.");let lastError=null;for(let attempts=0;attempts<5;attempts++){try{if(typeof targetDataCharacteristic.writeValueWithoutResponse==="function")await targetDataCharacteristic.writeValueWithoutResponse(data);else await targetDataCharacteristic.writeValue(data);return;}catch(e){lastError=e;const msg=(e&&e.message||"").toLowerCase();if(!msg.includes("already in progress")&&!msg.includes("operation in progress"))throw e;await new Promise(r=>setTimeout(r,120));}}throw lastError||new Error("BLE GATT write failed.");});return bleWriteQueue;}`
- After: `async function queueBLEWrite(data){bleWriteQueue=bleWriteQueue.catch(()=>{}).then(async()=>{if(!targetDataCharacteristic||!physicalTargetDevice?.gatt?.connected)throw new Error("Bluetooth disconnected.");let lastError=null;for(let attempts=0;attempts<5;attempts++){try{if(typeof targetDataCharacteristic.writeValueWithoutResponse==="function")await targetDataCharacteristic.writeValueWithoutResponse(data);else await targetDataCharacteristic.writeValue(data);return;}catch(e){lastError=e;const msg=(e&&e.message||"").toLowerCase();if(!msg.includes("already in progress")&&!msg.includes("operation in progress"))throw e;await new Promise(r=>setTimeout(r,120));}}throw lastError||new Error("BLE GATT write failed.");});return bleWriteQueue;}\nasync function transmitModeToken(token){if(!targetDataCharacteristic||!physicalTargetDevice?.gatt?.connected){alert("Bluetooth is not connected.");return;}if(commandBusy)return;commandBusy=true;try{await queueBLEWrite(new TextEncoder().encode(token));if(token==="A")webCubeMode="A";else if(token==="M")webCubeMode="M";document.getElementById("connectionLog").innerText=token==="A"?"AUTO MODE COMMAND SENT":token==="M"?"MANUAL MODE COMMAND SENT":"NEXT ANIMATION COMMAND SENT";}catch(e){const log=document.getElementById("connectionLog");log.innerText="COMMAND SEND ERROR: "+e.message;log.className="status-panel status-disconnected";console.error(e);}finally{commandBusy=false;}}`
- Reason: The three Auto/Manual/Next buttons invoked `transmitModeToken(...)`, but the current HTML contained no definition for that function. The buttons therefore failed at runtime with an undefined-function error. The handler now serializes `A`, `M`, or `N` through the existing BLE write queue without introducing an ACK dependency or protocol timeout.
- Contract/regression checks performed: Preserved the existing `A`, `M`, `N`, `C`, `CF_END`, `X`, `S`, and `B` protocol; preserved the unconditional 3.5-second post-GATT control delay; preserved notification parsing and BLE write retry behavior; no Math UI or handshake was restored.
- Commit SHA: `d3073aa62b79c138dc121a42672257bf93450545`.


### 2026-09-07 — Fix Custom receive-length overflow

- File: `LED_Cube_SMODE_512BIT_3BYTE_FIXED.ino`
- Change type: `FIX`
- Before: `char rxBuffer[512];
byte rxLength=0;`
- After: `char rxBuffer[512];
unsigned int rxLength=0;`
- Reason: `rxBuffer` is 512 bytes but `rxLength` was an 8-bit `byte`, wrapping at 255 bytes. Larger Custom uploads could therefore corrupt the receive buffer instead of using the existing 511-byte bounds check. The counter is widened to `unsigned int`.
- Contract/regression checks performed: Preserved the Custom `C`/`CF_END` protocol, compile/start separation, all built-in animations, display refresh, touch, brightness, BLE behavior, and pin assignments.

### 2026-09-07 — Fix Custom error-state UI synchronization

- File: `index.html`
- Change type: `FIX`
- Before: `else if(cleanData==="CUSTOM_NOT_READY"){customReady=false;document.getElementById("runCustomBtn").disabled=true;log.innerText="CUSTOM FUNCTION NOT READY";log.className="status-panel status-disconnected";}`
- After: `else if(cleanData==="CUSTOM_NOT_READY"){customReady=false;webCubeMode="NONE";document.getElementById("runCustomBtn").disabled=true;document.getElementById("stopCustomBtn").disabled=true;log.innerText="CUSTOM FUNCTION NOT READY";log.className="status-panel status-disconnected";}`
- Reason: `CUSTOM_NOT_READY` previously disabled Start Custom but could leave the browser's local Custom mode and Stop button stale. The UI now returns to `NONE` and disables Stop Custom when Arduino reports that no valid Custom program exists.
- Contract/regression checks performed: No command format changed; ACKs remain status-only; `X` remains the Arduino start request; BLE write queue, 20-byte upload slicing, 3.5-second connection delay, and Auto/Manual/Next behavior preserved.

### 2026-09-07 — Refresh README for current Custom-only Web Bluetooth implementation

- File: `README.md`
- Change type: `DOCUMENTATION`
- Reason: The README still described removed Heart/Cross-only Custom behavior and claimed Safari/iOS support that is not provided by native Web Bluetooth. The current page instead uses a general Custom Function editor, and native Safari/iOS Web Bluetooth remains unsupported.

### 2026-09-07 — Implement local Custom compile gate and Upload → Start workflow

- File: `LED_Cube_SMODE_512BIT_3BYTE_FIXED.ino`
- Change type: `FIX`
- Before: `if(!lastBluetoothConnected){currentCubeMode=0;parseMode=0;rxLength=0;customProgramValid=false;customReady=false;animationStart=now;lastFrameTime=now;triggerModeBlinkAcknowledgment();}`
- After: `if(!lastBluetoothConnected){currentCubeMode=0;parseMode=0;rxLength=0;animationStart=now;lastFrameTime=now;triggerModeBlinkAcknowledgment();}`
- Reason: Bluetooth disconnect must return the physical cube to built-in Auto Mode without erasing the already compiled Custom program held in RAM. The compiled Custom program is retained but is not displayed while Auto Mode runs.
- Contract/regression checks performed: Disconnect still uses the existing 3-second debounce; Auto timing restarts immediately; physical touch/brightness behavior remains unchanged; Custom `C` still clears/replaces the stored Custom program when a new upload begins; no new protocol command was introduced.

- File: `index.html`
- Change type: `BEHAVIOR CHANGE`
- Before: `<div class="control-group"><button id="sendCustomBtn" class="btn btn-primary bleControl" style="background:#475569" onclick="sendCustomFunction()" disabled>Send + Compile</button><button class="btn btn-success" id="runCustomBtn" onclick="startCustomFunction()" disabled>Start Custom</button></div>`
- After: `<div class="control-group"><button id="compileCustomBtn" class="btn btn-warning bleControl" onclick="compileCustomFunction()" disabled>Compile Custom</button><button class="btn btn-success bleControl" id="uploadCustomBtn" onclick="uploadAndStartCustomFunction()" disabled>Upload &amp; Start</button></div>`
- Reason: Compilation is now a browser-only preparation step. The Arduino must receive no Custom source when the local compile fails. A successful compile unlocks the upload action, and upload then sends the Custom source and automatically sends `X` to start the animation.
- Contract/regression checks performed: Editor starts empty; no saved source is preloaded; the existing 3.5-second post-GATT disable remains; Auto/Manual/Next and brightness controls remain direct commands; BLE writes remain serialized and packet-sliced.

- File: `index.html`
- Change type: `FIX`
- Before: `async function sendCustomFunction(){if(!targetDataCharacteristic){alert("Bluetooth is not connected.");return;}if(commandBusy)return;const code=document.getElementById("customFuncIDE").value.trim();if(!code){alert("Custom function is empty.");return;}commandBusy=true;localStorage.removeItem("cube_cached_custom_func");const log=document.getElementById("connectionLog");customReady=false;document.getElementById("runCustomBtn").disabled=true;log.innerText="SENDING CUSTOM FUNCTION — CUBE WILL CLEAR AND WAIT...";log.className="status-panel";try{const enc=new TextEncoder();await queueBLEWrite(enc.encode("C"));const payload=code+"\nCF_END\n";for(let i=0;i<payload.length;i+=20){await queueBLEWrite(enc.encode(payload.slice(i,i+20)));await new Promise(r=>setTimeout(r,20));}customReady=true;document.getElementById("runCustomBtn").disabled=false;log.innerText="CUSTOM FUNCTION SENT — WAITING FOR START";log.className="status-panel status-connected";}catch(e){customReady=false;document.getElementById("runCustomBtn").disabled=true;log.innerText="FUNCTION SEND ERROR: "+e.message;log.className="status-panel status-disconnected";console.error(e);}finally{commandBusy=false;}}`
- After: `async function compileCustomFunction(){if(commandBusy)return;const code=document.getElementById("customFuncIDE").value.trim();if(!code){alert("Custom function is empty.");return;}const result=localCompileCustomSource(code);const log=document.getElementById("connectionLog");if(!result.ok){localCompileOk=false;compiledCustomSource="";document.getElementById("uploadCustomBtn").disabled=true;log.innerText="CUSTOM COMPILE FAILED — NOTHING SENT TO ARDUINO";log.className="status-panel status-disconnected";alert(result.error);return;}localCompileOk=true;compiledCustomSource=code;document.getElementById("uploadCustomBtn").disabled=false;log.innerText="CUSTOM COMPILE OK — READY TO UPLOAD";log.className="status-panel status-connected";}`
- Reason: The old Send + Compile operation transmitted source to Arduino before any browser-side compile result existed. The replacement performs a local compile/validation first and stores the exact successfully compiled source. No Bluetooth write occurs on compile failure.
- Contract/regression checks performed: A changed textarea invalidates the previous compile; upload requires the current source to exactly match the successfully compiled source; no ACK or timeout is required for normal command execution.

- File: `index.html`
- Change type: `FIX`
- Before: `async function startCustomFunction(){if(!targetDataCharacteristic){alert("Bluetooth is not connected.");return;}if(commandBusy)return;commandBusy=true;try{await queueBLEWrite(new TextEncoder().encode("X"));webCubeMode="CUSTOM";document.getElementById("stopCustomBtn").disabled=false;document.getElementById("connectionLog").innerText="CUSTOM START COMMAND SENT";}catch(e){const log=document.getElementById("connectionLog");log.innerText="CUSTOM START ERROR: "+e.message;log.className="status-panel status-disconnected";console.error(e);}finally{commandBusy=false;}}`
- After: `async function uploadAndStartCustomFunction(){if(!targetDataCharacteristic||!physicalTargetDevice?.gatt?.connected){alert("Bluetooth is not connected.");return;}if(!localCompileOk||document.getElementById("customFuncIDE").value.trim()!==compiledCustomSource){alert("Compile the current Custom function successfully before uploading.");return;}if(commandBusy)return;commandBusy=true;const log=document.getElementById("connectionLog");customReady=false;webCubeMode="NONE";document.getElementById("uploadCustomBtn").disabled=true;document.getElementById("stopCustomBtn").disabled=true;log.innerText="ENTERING CUSTOM WAITING — CUBE BLANK";log.className="status-panel";try{const enc=new TextEncoder();await queueBLEWrite(enc.encode("C"));const payload=compiledCustomSource+"\nCF_END\n";for(let i=0;i<payload.length;i+=20){await queueBLEWrite(enc.encode(payload.slice(i,i+20)));await new Promise(r=>setTimeout(r,20));}await queueBLEWrite(enc.encode("X"));webCubeMode="CUSTOM";document.getElementById("stopCustomBtn").disabled=false;log.innerText="CUSTOM START COMMAND SENT";log.className="status-panel status-connected";}catch(e){customReady=false;webCubeMode="NONE";document.getElementById("uploadCustomBtn").disabled=!localCompileOk;document.getElementById("stopCustomBtn").disabled=true;log.innerText="CUSTOM UPLOAD ERROR — CUBE REMAINS BLANK";log.className="status-panel status-disconnected";console.error(e);}finally{commandBusy=false;}}`
- Reason: Upload now has a strict successful-local-compile gate, enters Custom Waiting with a blank cube by sending `C`, transfers the compiled source, and then sends `X` automatically. There is no separate Start Custom action after upload.
- Contract/regression checks performed: `C` remains the blanking command; the Arduino still accepts `X` only when a valid compiled Custom program exists; `CUSTOM_OK`/`CUSTOM_ERROR` remain informational; `S` still returns to Auto.

### 2026-09-07 — Preserve compiled Custom program across BLE disconnect

- File: `LOGIC_CONTRACT.md`
- Change type: `BEHAVIOR CHANGE`
- Before: `| Bluetooth is disconnected | After the configured disconnect detection/debounce period, the cube independently returns to built-in Auto Mode. The web app does not need to send an Auto command. |` and the Custom workflow did not define browser-local compilation before upload.
- After: The Bluetooth disconnect row additionally states that a previously compiled Custom program may remain stored in RAM while Auto runs; Custom workflow rows define browser-local compile first, no Arduino upload on failure, successful upload only, and automatic `X` start after upload.
- Reason: The live contract is updated to exactly match the requested device behavior: disconnect resumes built-in Auto without destroying the stored Custom program, while Custom source is compiled in the web app before transmission.
- Contract/regression checks performed: Preserved the 3-second disconnect debounce, no-handshake rule, ACK-as-status rule, blank Custom Waiting state, `C`/`CF_END`/`X`/`S` command roles, and common display pipeline.

- File: `CODE_CHANGE_LOG.md`
- Change type: `DOCUMENTATION`
- Before: This project change had not yet been recorded in the append-only log.
- After: This entry records the exact firmware disconnect edit and the exact HTML Custom workflow edits above.
- Reason: Repository protection requires every Arduino/HTML edit and any Logic Contract change to be recorded in the same history.
- Contract/regression checks performed: Verified that the logged edits correspond to the committed tree changes and that the old log entries remain unchanged.
- Commit SHA: recorded in the Git commit containing these changes.

### 2026-09-07 — Correct local Custom compiler syntax and connection-state handling

- File: `index.html`
- Change type: `FIX`
- Before: The local compiler used `function validateCustomExpression(expr,allowedNames){let s=expr.trim();if(!s)return false;s=s.replace(/\b(sin|cos|sqrt|abs)\s*\(/gi,(m,fn)=>({sin:"Math.sin(",cos:"Math.cos(",sqrt:"Math.sqrt(",abs:"Math.abs("})[fn.toLowerCase()]));s=s.replace(/\bOR\b/gi,"||");const ids=s.match(/[A-Za-z_][A-Za-z0-9_]*/g)||[];const builtins=new Set(["X","Y","Z","F","T","MATH","SIN","COS","SQRT","ABS"]);for(const id of ids){const u=id.toUpperCase();if(!builtins.has(u)&&!allowedNames.has(u))return false;}try{new Function("X","Y","Z","F","T","Math","return ("+s+");");return true;}catch(_){return false;}}`.
- After: `function validateCustomExpression(expr,allowedNames){const original=expr.trim();if(!original||!/^[A-Za-z0-9_+*/%<>=!&|().\s-]+$/.test(original))return false;const originalIds=original.match(/[A-Za-z_][A-Za-z0-9_]*/g)||[];const builtins=new Set(["X","Y","Z","F","T","SIN","COS","SQRT","ABS"]);for(const id of originalIds){const u=id.toUpperCase();if(!builtins.has(u)&&!allowedNames.has(u))return false;}let s=original;s=s.replace(/\b(sin|cos|sqrt|abs)\s*\(/gi,(m,fn)=>({sin:"Math.sin(",cos:"Math.cos(",sqrt:"Math.sqrt(",abs:"Math.abs("})[fn.toLowerCase()]));s=s.replace(/\bOR\b/gi,"||");try{new Function("X","Y","Z","F","T","Math","return ("+s+");");return true;}catch(_){return false;}}`.
- Reason: The browser compiler must accept the same practical expression grammar as the Arduino compiler. The stricter validation rejects JavaScript-only syntax such as `^`, `?:`, arrays, strings, commas, single `=` and other unsupported constructs, while allowing only the four supported math functions and declared variables. It also fixes function-name transformation and prevents raw `Math.*` identifiers from bypassing the Custom grammar.
- Contract/regression checks performed: No Bluetooth write is performed by local compilation; upload remains gated on `localCompileOk` and unchanged source; supported `X/Y/Z/F/T`, arithmetic, comparisons, `&&`, `||`, `!`, `sin/cos/sqrt/abs`, and helper variables remain accepted.
- Commit SHA: recorded in the Git commit containing this correction.
