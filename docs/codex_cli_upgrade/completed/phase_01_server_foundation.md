# phase_01_server_foundation

## goal
Create the minimal reliable IPC bridge between the running IDE instance and a standalone CLI so we can prove connectivity and protocol correctness with `ping`.

## scope
- In scope: server lifecycle, socket lifecycle, protocol envelope, CLI connectivity, ping command.
- Out of scope: diagnostics, symbols, open, build, patch apply.

## execution checklist
1. [x] choose and lock phase-1 CLI binary name
2. [x] add `src` module for IDE IPC server lifecycle (init, start, stop, cleanup)
3. [x] add per-session socket path generator under `~/.cache/<app_name>/sock/`
4. [x] enforce restrictive socket file permissions
5. [x] inject `MYIDE_SOCKET` and `MYIDE_PROJECT_ROOT` into PTY shell environment
6. [x] add request accept loop with multi-client support
7. [x] add non-blocking dispatch model (worker thread or queue)
8. [x] define `proto=1` JSON request envelope parser
9. [x] define `proto=1` JSON response envelope serializer
10. [x] implement command dispatcher with unknown-command structured error
11. [x] implement `ping` handler returning session metadata and server version
12. [x] create standalone CLI executable with subcommand parser
13. [x] implement CLI connection path using `MYIDE_SOCKET`
14. [x] implement `ping` subcommand in CLI with `--json` and human modes
15. [x] define exit code contract (success, usage error, connect error, protocol error, server error)
16. [x] add baseline tests for parsing, protocol envelope validation, and ping flow
17. [x] run build and targeted tests, then fix regressions
18. [x] update this phase doc with completion checks and move to `completed/` when done

## deliverable contracts

### request envelope
- [x] `id` string
- [x] `proto` integer, phase 1 fixed to `1`
- [x] `cmd` string
- [x] `args` object

### response envelope
- [x] `id` string (echo request id)
- [x] `ok` boolean
- [x] `result` object for success
- [x] `error` object for failure with stable fields: `code`, `message`, optional `details`

## `ping` result contract
- [x] include protocol version
- [x] include IDE process id
- [x] include session identifier
- [x] include project root
- [x] include server version string
- [x] include server timestamp or uptime field

## test checklist
1. [x] no `MYIDE_SOCKET` set returns clear error and non-zero exit
2. [x] invalid socket path returns clear connect failure and non-zero exit
3. [x] valid socket responds to `ping` in human mode
4. [x] valid socket responds to `ping --json` with valid JSON
5. [x] malformed request returns structured `ok=false` response
6. [x] unknown command returns structured `ok=false` response
7. [x] server startup and shutdown cleans socket file correctly

## phase completion gate
- [x] all execution checklist items completed
- [x] all test checklist items completed
- [x] `docs/codex_cli_upgrade/north_star.md` phase_01 marked complete
- [x] this doc moved to `docs/codex_cli_upgrade/completed/`
