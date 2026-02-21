# phase_05_ux_and_scale

## goal
Improve reliability and scale of the IDE bridge CLI by adding robust IPC timeout behavior, clearer failure taxonomy, Linux-compatible socket path handling, and optional large-output spill mode.

## scope
- In scope: CLI transport resiliency, output spill ergonomics, socket path portability, help/usage polish.
- Out of scope: protocol version changes and new IDE data commands.

## execution checklist
1. [x] add `idebridge --socket <path>` override support (env fallback preserved)
2. [x] add IPC timeout controls and deterministic timeout handling
3. [x] strengthen IPC error taxonomy and user-facing error messages
4. [x] add optional response spill-to-file mode for large JSON output
5. [x] improve CLI help text for global options and command examples
6. [x] update IDE socket path resolution for Linux compatibility (`XDG_CACHE_HOME` + fallback)
7. [x] add phase-5 runtime test coverage for socket override and spill mode
8. [x] run build and phase tests, then fix regressions
9. [x] update north star phase status and move this doc to `completed/`

## contracts

### cli flags
- [x] `--socket` explicitly selects socket path
- [x] `--timeout_ms` controls request timeout bound
- [x] `--spill_file` writes raw JSON response payload to disk

### error behavior
- [x] timeout failures return deterministic timeout exit code
- [x] connection/protocol/server failures remain distinguished by exit code
- [x] help output documents exit code meaning

## test checklist
1. [x] `idebridge ping --socket <path>` succeeds against active IPC server
2. [x] missing/invalid socket override fails with deterministic non-zero code
3. [x] `--spill_file` writes valid JSON payload for a successful command
4. [x] socket creation path works with Linux-compatible cache root fallback logic

## phase completion gate
- [x] all execution checklist items completed
- [x] all test checklist items completed
- [x] `docs/codex_cli_upgrade/north_star.md` phase_05 marked complete
- [x] this doc moved to `docs/codex_cli_upgrade/completed/`
