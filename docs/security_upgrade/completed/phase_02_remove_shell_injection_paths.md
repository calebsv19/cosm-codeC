# Phase 02 - Remove Shell Injection Paths (`SEC-002`)

Status: `completed`
Owner: `IDE core / IPC`
Depends on: `completed/phase_01_ipc_workspace_confinement.md`

## Objective

Reduce command injection risk in IPC-triggered build execution while preserving normal build behavior.

## Scope

In-scope:

- Harden IPC `build` command handling in `ide_ipc_server`.
- Eliminate shell interpolation of IPC `profile`.
- Use non-shell `exec` path for default `make` build.

Out-of-scope (later phase/refactor):

- Full removal of shell from all custom build command paths.
- Redesign of workspace build configuration schema.

## Step Plan (Execution Checklist)

1. [x] Add strict profile allowlist validation (`debug`, `perf`).
2. [x] Remove direct profile interpolation into shell command strings.
3. [x] Add `fork/exec` + pipe capture for `make` path (no shell).
4. [x] Keep diagnostics parsing/output behavior equivalent.
5. [x] Preserve fallback/custom command behavior with safer profile handling.
6. [x] Compile changed IPC objects and verify no regressions.

## Acceptance Criteria

- Invalid profile payload is rejected with a clear error.
- Default make build path no longer runs via `popen("...make...")`.
- Build output and diagnostics summary are still returned in IPC response.
- No changes to successful baseline build behavior for normal profiles.

## Implementation Notes

- Updated `src/core/Ipc/ide_ipc_server.c`:
  - Added profile allowlist check.
  - Added non-shell `make` execution path (`fork` + `execvp`) with stdout/stderr capture.
  - Removed profile string interpolation from shell command construction.
  - Preserved output aggregation and diagnostics feed behavior.

## Validation Performed

- Compiled:
  - `build/debug/core/Ipc/ide_ipc_server.o`
