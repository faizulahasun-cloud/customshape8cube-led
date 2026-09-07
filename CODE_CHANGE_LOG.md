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
