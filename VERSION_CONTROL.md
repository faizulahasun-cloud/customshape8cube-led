# 8x8x8 LED Cube — Version Control Rules

This file protects the project's important logic from being lost during future edits.

## 1. Two documents, two jobs

| File | Job | Rule |
|---|---|---|
| `LOGIC_CONTRACT.md` | Current required behavior | Update only when the intended project behavior changes |
| `VERSION_CONTROL.md` | Rules for protecting the logic | Do not remove these rules casually |
| `CODE_CHANGE_LOG.md` | Permanent edit history | Append only; preserve every historical entry |

`LOGIC_CONTRACT.md` is the live specification.
`VERSION_CONTROL.md` explains how to edit code without losing that specification.
`CODE_CHANGE_LOG.md` is the permanent record of every code edit and its exact Before → After → Reason.

## 2. Never silently change behavior

A code edit must not remove, weaken, rename, or bypass an existing behavior just because the new feature works.

Examples of protected behavior:

- Bluetooth disconnect returns Arduino to built-in Auto Mode.
- Handshake uses `H` → `HANDSHAKE_OK`.
- ACK waiter is created before sending a command.
- Protocol bytes are not LED frame bytes.
- Built-in, Math, and Custom rendering use the same display pipeline.
- Front face remains `y=0`.
- Rotating heart remains animation function/index 26.
- RAM-saving function-based animation design is preserved.

## 3. Change classification

Every requested change should be treated as one of these:

| Type | Meaning | What to do |
|---|---|---|
| FIX | Correct a bug without changing intended behavior | Preserve the contract; update code only |
| FEATURE | Add new capability | Add it without breaking existing rules |
| BEHAVIOR CHANGE | Intentionally change an existing rule | Update `LOGIC_CONTRACT.md` and record the reason in Git history |
| REFACTOR | Change implementation only | Output behavior must remain the same |

Do not treat a behavior change as a normal refactor.

## 4. Safe editing procedure

Before editing `index.html` or the Arduino `.ino`:

1. Read the current code from GitHub.
2. Read `LOGIC_CONTRACT.md`.
3. Identify which contract rules the requested change touches.
4. Modify only the necessary code.
5. Check that unrelated functions and protocols are still present.
6. Re-check every item in the Future Change Checklist in `LOGIC_CONTRACT.md`.
7. Append the exact Before → After → Reason entry to `CODE_CHANGE_LOG.md` in the same change.
8. Commit with a clear description of what changed.

## 5. Do not use an old file as the source of truth

The latest files on GitHub are the source of truth.

Do not rebuild a file from an old conversation message, old code block, or memory of a previous version when the current GitHub file can be read.

This prevents old logic from overwriting newer fixes.

## 6. Git history is part of the protection system

Git commits provide historical versions of the code.

For an important logic change:

- Keep the commit focused.
- Use a descriptive commit message.
- Do not combine unrelated feature changes with logic-protection changes when avoidable.
- Before a risky change, compare the current version with the previous version.

Examples of useful commit messages:

- `Reduce Arduino SRAM usage`
- refresh rate

## 7. Logic document versioning

`LOGIC_CONTRACT.md` should contain the current rules only.

When an intended behavior changes:

1. Change the affected rule in `LOGIC_CONTRACT.md`.
2. Explain the change in the Git commit message.
3. Keep the old behavior available in Git history; do not erase the historical evidence.

Do not create dozens of duplicate copies such as `LOGIC_CONTRACT_v2`, `v3`, `final`, `final2`, etc. Git already provides the historical versions.

## 8. Code must be checked against the contract, not only compiled

A successful compile does NOT prove that the project logic is still correct.

A change is not considered safe merely because:

- Arduino compiles.
- The webpage loads.
- Bluetooth connects once.
- One animation works.

The existing contract must also remain satisfied.

## 9. Regression check after every code modification

After modifying either major code file, check at minimum:

| Area | Verify |
|---|---|
| Bluetooth | Connect, handshake, commands, notifications, disconnect fallback |
| Modes | Auto, Manual, Math, Custom |
| Commands | Every command still has its intended meaning |
| ACKs | ACK is still produced, parsed, and waited for correctly |
| Display | Shift registers still receive only display data |
| Coordinates | X/Y/Z contract unchanged unless intentionally changed |
| Animations | Existing animations remain functions and still render |
| Memory | No unnecessary frame tables or duplicated buffers added |
| Recovery | Bluetooth loss still causes the intended local Arduino behavior |

## 10. If something is removed, prove it was intentional

If a future edit removes an existing function, variable, command, fallback, or protocol rule, there must be a clear reason.

Do not delete something only because it looks unused without checking `LOGIC_CONTRACT.md` and the whole code path first.

## 11. Permanent code-change log protection

`CODE_CHANGE_LOG.md` is append-only historical evidence.

For every edit to `LED_Cube_SMODE_512BIT_3BYTE_FIXED.ino` or `index.html`:

- Append an entry to `CODE_CHANGE_LOG.md` in the same commit.
- Record the exact Before text.
- Record the exact After text.
- Record the reason for the replacement.
- Record the contract/regression checks performed.
- Never delete, rewrite, reorder, or silently alter prior entries.
- The assistant must not delete the log file or historical entries. Deletion is reserved for the repository owner/user.

Analysis/check/report without modification does not create a code-change entry.

## Golden rule

**Current GitHub code + `LOGIC_CONTRACT.md` are the source of truth. `CODE_CHANGE_LOG.md` is the permanent exact edit history. Git history is the backup history. Never reconstruct the project from memory when the repository can be inspected.**
