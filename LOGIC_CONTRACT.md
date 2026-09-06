# 8x8x8 LED Cube — Logic Contract

This file records the important behavior that must NOT be accidentally removed or changed during future code edits.

Before changing the Arduino or web code, check this table first.

## 0. Modification authorization rule

**Analysis does not equal permission to modify.**

Before changing any project file:

1. Analyze the current Arduino/HTML content first.
2. Identify whether a change is actually required for the user's requested task.
3. Modify **only** the file(s) the user explicitly authorized for modification.
4. Never modify Arduino, HTML, or another project file merely because the analysis finds an improvement, inconsistency, cleanup opportunity, or missing implementation.
5. A rule written in this contract is a **behavior requirement**, not permission to implement or change it without user authorization.
6. When the user says **analyze/check/report**, do not modify code.
7. When the user authorizes an MD/document-only change, modify only that requested documentation file.

When in doubt, preserve the existing code and report the finding instead of changing it.

## 1. Main control logic

| Rule | Required behavior | Where it belongs |
|---|---|---|
| Built-in Auto Mode | `currentCubeMode == 0` runs the built-in animation system | Arduino |
| Manual Mode | `currentCubeMode == 1` runs the selected built-in animation and allows Next Animation | Arduino |
| Math Mode | `currentCubeMode == 3` renders the uploaded math expression on Arduino | Arduino |
| Custom Mode | `currentCubeMode == 4` renders the uploaded custom function on Arduino | Arduino |
| Animation rendering | Built-in, Math, and Custom animations must use the same cube frame/display pipeline | Arduino |
| Frame generation | Generate the current frame in RAM, then copy it atomically into `displayBuffer` | Arduino |

## 2. Arduino startup / power-up

| Rule | Required behavior | Important detail |
|---|---|---|
| Power-up delay | After Arduino startup, wait **3 seconds** before starting the built-in Auto animation | This gives the power-supply / capacitor stage time to charge before the cube starts scanning LEDs |
| Startup mode | After the 3-second delay, the Arduino starts in built-in Auto Mode | Do not remove the startup delay during normal code edits |
| Startup safety | The 3-second delay is a startup requirement only; it does not replace the normal Bluetooth-disconnect fallback | Keep the two behaviors separate |

## 3. Bluetooth connection / disconnection

| Rule | Required behavior | Important detail |
|---|---|---|
| Bluetooth connection | Web page connects to HM-10 using the FFE0 service and FFE1 characteristic | Do not silently change these UUIDs |
| Handshake | Web page sends `H`; Arduino replies `HANDSHAKE_OK` | Handshake must be reliable and have retry protection |
| Notifications | Web page enables notifications before sending the handshake | Required so the ACK can be received |
| BLE disconnect | Arduino detects HM-10 STATE becoming disconnected | Uses `BLE_STATE_PIN` |
| Disconnect debounce | Do not immediately switch modes; require the configured debounce period | Current code uses 3 seconds |
| Automatic fallback | After a real Bluetooth disconnect, Arduino sets `currentCubeMode = 0` | This returns the cube to built-in Auto Mode |
| Web disconnect | Web page must clear its session state and disable controls | Arduino still independently handles Auto fallback |
| No disconnect command required | Web page does NOT need to send `A` when Bluetooth disconnects | Arduino decides the fallback locally |

## 4. Bluetooth command protocol

| Command from web | Arduino action | Expected ACK |
|---|---|---|
| `H` | Handshake | `HANDSHAKE_OK` |
| `A` | Select built-in Auto Mode | `MODE_AUTO` |
| `M` | Select Manual Mode | `MODE_MANUAL` |
| `N` | Next built-in animation (Manual Mode only) | `ANIMATION_NEXT` |
| `Y` | Start Math upload channel | `MATH_UPLOAD_READY` |
| Math text + newline | Compile Math expression | `MATH_OK` or `MATH_ERROR` |
| `F` | Start Math Mode | `MATH_STARTED` or `MATH_NOT_READY` |
| `C` | Start Custom upload channel | `CUSTOM_UPLOAD_READY` |
| Custom text + `CF_END` | Compile Custom function | `CUSTOM_OK` or `CUSTOM_ERROR` |
| `X` | Start Custom Mode | `CUSTOM_STARTED` or `CUSTOM_NOT_READY` |
| `S` | Stop Custom Mode | `CUSTOM_STOPPED` or `CUSTOM_NOT_ACTIVE` |
| `B` + one byte | Set brightness | `BRIGHTNESS_OK` or `BRIGHTNESS_ERROR` |

## 5. ACK / transaction rule

Every web command that expects an ACK should follow this order:

**1. Create the ACK waiter → 2. Send the command → 3. Wait for the ACK.**

Do NOT send the command first and create the ACK waiter afterward. That can create a race where the HM-10/Arduino replies before the web page starts listening for that specific ACK.

Commands that currently use ACK transactions:

- Handshake
- Auto
- Manual
- Next Animation
- Math upload/start
- Custom upload/start/stop

## 6. HM-10 communication reliability

| Rule | Required behavior |
|---|---|
| GATT settle time | Allow a short delay after GATT connection/notifications before the initial handshake |
| Handshake retry | Retry the handshake instead of failing on one missed response |
| BLE write retry | Retry transient GATT busy / operation-in-progress errors |
| Write compatibility | Prefer `writeValueWithoutResponse()` but support `writeValue()` fallback when necessary |
| Notification parser | Treat incoming notifications as a text stream; handle partial packets and newline-delimited ACKs |

## 7. Frame / shift-register rule

The Bluetooth command bytes are **protocol bytes**, not LED frame bytes.

`H`, `A`, `M`, `N`, `Y`, `F`, `C`, `X`, `S`, and `B` are interpreted by the Arduino command parser. They are NOT sent directly to the 74HC595 shift registers as display data.

The shift registers receive only the actual display data produced from `displayBuffer` by the refresh routine.

## 8. Cube coordinate contract

| Coordinate | Meaning |
|---|---|
| X | Columns 1–8, left to right on the physical FRONT face |
| Y | Depth, FRONT face = `y=0`, rear face = `y=7` |
| Z | Vertical height, bottom = `z=0`, top = `z=7` |

All built-in, Math, and Custom animations must use this same coordinate system.

## 9. Physical front-face mapping

- The first 8 physical columns are the FRONT face columns 1–8.
- `COLUMN_MAP` is the single mapping used to convert `(x,y)` to shift-register register/bit positions.
- Do not create a second conflicting column mapping for an individual animation.

## 10. Rotating-heart rule

The rotating heart is a normal built-in-style animation function, not a separate frame upload system.

- Animation index: `26`
- It uses the same `animationVoxel()` → `drawAnimationFrame()` → `displayBuffer` pipeline as the other built-in animations.
- The heart is on the physical FRONT face (`y=0` and `y=1`), using X-Z as its visible plane.
- Its rotation must respect the physical front-face coordinate contract.

## 11. Memory / RAM protection

The Arduino is RAM-limited. Future changes must avoid unnecessarily large frame arrays or duplicated animation storage.

Preferred design:

**Function → calculate voxel state → build one 8×8 layer-buffer frame → atomically copy to `displayBuffer` → refresh through shift registers.**

Do not replace function-based animations with large stored frame tables unless there is a clear memory/functional reason.

Current important RAM structures include:

- `displayBuffer[8][8]`
- packed `mathProgram[96]`
- `rxBuffer[512]`

Keep memory usage in mind whenever adding features.

## 12. Refresh / display rule

The refresh interrupt independently scans the cube layers and shifts data to the 74HC595 chain.

Animation code should update `displayBuffer`; it should not directly control the shift-register pins during normal animation rendering.

## 13. Device / user behavior contract

This table describes the intended physical behavior of the finished cube and its control page. These behaviors must be preserved during future changes.

| Device behavior | Required behavior | Important detail |
|---|---|---|
| Arduino power-up | Wait **3 seconds**, then start the cube in built-in **Auto Mode** | The delay is for capacitor / power-supply charging before normal LED scanning starts |
| Long touch | Holding the physical touchpad for **more than 3 seconds** toggles between Auto Mode and Manual Mode | When entering Manual Mode, keep the **current animation**; do not automatically advance it |
| Short / single touch | A normal single touch advances to the **next animation** | Only acts as Next Animation while in **Manual Mode**; it does not change animation during Auto Mode |
| Touch while Bluetooth is connected | Physical touch input is ignored | The connected web interface controls the cube instead |
| Brightness without Bluetooth | The physical **potentiometer** controls LED brightness | Arduino continuously reads `POT_PIN` while disconnected |
| Brightness with Bluetooth | Web interface brightness control sends brightness to Arduino | Arduino uses the received brightness value instead of the disconnected-state potentiometer control |
| Bluetooth connect | Web interface establishes the HM-10 session, verifies handshake, and enables controls | Existing animation state is not replaced merely by connecting |
| Bluetooth Auto command | Selecting Auto from the web page switches Arduino to built-in Auto Mode | ACK: `MODE_AUTO` |
| Bluetooth Manual command | Selecting Manual from the web page switches Arduino to Manual Mode | ACK: `MODE_MANUAL`; current animation remains selected |
| Web Next command | Next Animation advances the selected built-in animation | Intended for Manual Mode; ACK: `ANIMATION_NEXT` |
| Math mode | Web page uploads an expression, then starts Math Mode; Arduino renders the expression locally | Uses the Arduino math engine and common display pipeline |
| Custom mode | Web page uploads a custom function, then starts Custom Mode; Arduino renders it locally | Stop returns the cube to Auto Mode |
| Bluetooth disconnect | After the configured 3-second BLE disconnect debounce, Arduino returns to built-in Auto Mode | This is handled locally by Arduino; the web page does not need to send `A` |

**Important touch rule:** a single touch is not a second way to enter Manual Mode. A long touch changes mode; a short touch performs Next Animation only when already in Manual Mode.

## 14. Future-change checklist

Before committing a change, verify:

| Check | Must remain true |
|---|---|
| Modification authorization | Do not modify a file unless the user explicitly authorized that file/change |
| Startup delay | Arduino waits 3 seconds after power-up before starting built-in Auto animation |
| Startup mode | Cube starts in built-in Auto Mode after the startup delay |
| Long touch | More than 3 seconds toggles Auto/Manual without changing the selected animation |
| Single touch | Short touch advances to the next animation only in Manual Mode |
| Touch priority | Touch input is ignored while Bluetooth is connected |
| Potentiometer | Physical potentiometer controls brightness while Bluetooth is disconnected |
| Web brightness | Connected web control sets brightness through the `B` protocol |
| Disconnect fallback | Bluetooth loss still returns to built-in Auto Mode |
| Handshake | `H` still gets `HANDSHAKE_OK` reliably |
| ACK order | Waiter is created before command is sent |
| ACK parsing | Newline-delimited replies still work even when notifications arrive in chunks |
| Command bytes | Protocol commands are never treated as LED frame data |
| Auto/Manual | Built-in modes still work independently of Math/Custom uploads |
| Math | Upload, compile, and start sequence still works |
| Custom | Upload, compile, start, and stop sequence still works |
| Brightness | `B` + value remains the brightness protocol |
| Coordinate system | Front face remains `y=0`, X is left-right, Z is vertical |
| Animation pipeline | Custom/built-in animations use the same frame/display pipeline |
| Memory | No unnecessary large frame arrays are introduced |
| Shift register output | Only display data reaches the 74HC595 refresh path |

## Golden rule

**Do not judge a future change only by whether the new feature works. Also verify that every existing rule in this document still works.**

When modifying either file, review this contract first and specifically check for missing logic, removed fallback behavior, changed command meanings, ACK races, unauthorized file changes, startup delay removal, touch-control changes, brightness-control changes, coordinate changes, and unnecessary RAM growth.
