# Phase 01 - IPC Workspace Confinement (`SEC-001`)

Status: `completed`
Owner: `IDE core / IPC`
Depends on: `docs/security_upgrade/north_star.md`

## Objective

Constrain IPC edit/hash file targets to canonical workspace paths so patch apply cannot write outside the active project root.

## Why This Phase First

- This is the highest data-integrity risk in the security report.
- It is a bounded change with low blast radius.
- It preserves existing IDE UX for valid workspace edits.

## Step Plan (Execution Checklist)

1. [x] Add shared path-guard helper for IPC file targets.
2. [x] Reject absolute request paths for IPC edit/hash target resolution.
3. [x] Resolve canonical workspace root and canonical target path with `realpath`.
4. [x] Enforce canonical prefix check (`target` must remain under `workspace`).
5. [x] Wire guard into `ide_ipc_apply_unified_diff`.
6. [x] Wire guard into `verify_edit_hashes`.
7. [x] Compile changed IPC units to confirm no integration regressions.

## Implementation

New shared helper:

- `src/core/Ipc/ide_ipc_path_guard.h`
- `src/core/Ipc/ide_ipc_path_guard.c`

Call-site integrations:

- `src/core/Ipc/ide_ipc_edit_apply.c`
- `src/core/Ipc/ide_ipc_server.c`

## Behavior Changes

Allowed:

- IPC edit apply for existing files inside workspace root.
- IPC hash verification for files inside workspace root.

Blocked:

- Absolute file paths in edit/hash requests.
- Any target that canonicalizes outside workspace root.

## Non-Regression Notes

- Valid in-workspace edit flow is unchanged.
- Error path is fail-closed with explicit message instead of silently applying.
- No UI behavior changes expected for normal IDE usage.

## Validation Performed

- Compiled:
  - `build/debug/core/Ipc/ide_ipc_path_guard.o`
  - `build/debug/core/Ipc/ide_ipc_edit_apply.o`
  - `build/debug/core/Ipc/ide_ipc_server.o`

## Next Phase

- Proceed to `SEC-002` (`Phase 2 - Remove Shell Injection Paths`).

