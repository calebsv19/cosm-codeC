# Phase 03 - IPC Auth and Capability Tiers (`SEC-003`)

Status: `completed`
Owner: `IDE core / IPC`
Depends on: `completed/phase_02_remove_shell_injection_paths.md`

## Objective

Require explicit authorization for mutating IPC commands while keeping read-only commands frictionless.

## Scope

In-scope:

- Add per-session IPC auth token generated at server start.
- Require auth token for mutating commands (`open`, `build`, `edit`).
- Keep read-only commands unauthenticated (`ping`, `diag`, `symbols`, `includes`, `search`).
- Propagate token into IDE-launched terminal environment for `idebridge`.

Out-of-scope:

- Cross-user/multi-tenant auth model.
- External credential provider or OS keychain integration.

## Step Plan (Execution Checklist)

1. [x] Add server-side auth token lifecycle (generate/store/clear/accessor).
2. [x] Add command-tier policy helper (read-only vs mutating).
3. [x] Enforce auth checks for mutating command handling in IPC request parser.
4. [x] Extend terminal backend spawn path to export token env var.
5. [x] Extend `idebridge` to send auth token (`--token` or `MYIDE_AUTH_TOKEN`).
6. [x] Compile affected IPC/terminal/idebridge units.

## Acceptance Criteria

- Mutating commands fail with auth error when token is missing/invalid.
- Read-only commands still work without token.
- IDE terminal sessions can still run `idebridge` mutating commands successfully.
- No behavior changes to non-security logic outside auth gating.

## Implementation Notes

- Updated `src/core/Ipc/ide_ipc_server.c`:
  - Added per-session auth token generation and lifecycle.
  - Added command-tier auth gate for `open`, `build`, `edit`.
  - Added accessor `ide_ipc_auth_token()`.
- Updated terminal bridge:
  - `src/core/Terminal/terminal_backend.h`
  - `src/core/Terminal/terminal_backend.c`
  - `src/ide/Panes/Terminal/terminal.c`
  - IDE terminal now exports `MYIDE_AUTH_TOKEN` for child shell tools.
- Updated `tools/idebridge/idebridge.c`:
  - Adds top-level `auth_token` field on requests when available.
  - Supports `--token` and `MYIDE_AUTH_TOKEN`.

## Validation Performed

- Compiled:
  - `build/debug/core/Ipc/ide_ipc_server.o`
  - `build/debug/core/Terminal/terminal_backend.o`
  - `build/debug/ide/Panes/Terminal/terminal.o`
  - `build/debug/tools/idebridge.o`
