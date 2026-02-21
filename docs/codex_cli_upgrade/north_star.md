# codex_cli_upgrade north star

## objective
Build a production-usable bridge between the IDE and Codex CLI via a Unix domain socket and a standalone CLI tool, so Codex can query IDE state and trigger IDE actions using normal shell commands.

## guiding principles
- IDE is source of truth for diagnostics, symbols, navigation, and edit application.
- Codex uses normal shell commands and stdout only.
- JSON contracts stay stable and versioned.
- Start with minimum useful surface, then expand.

## defaults and decisions
- [x] transport: Unix domain socket
- [x] socket root: `~/.cache/<app_name>/sock/`
- [x] PTY routing env vars: `MYIDE_SOCKET`, `MYIDE_PROJECT_ROOT`
- [x] platform order: macOS first, Linux compatibility tracked in phase 5
- [x] default output mode: full output, explicit truncation flags when needed
- [x] patch hash policy: required by default when patch apply is implemented
- [x] cli binary final name selected: `idebridge`
- [x] optional `--socket` override flag (planned after phase 1)

## phase map

### phase_01_server_foundation
status: [x]

deliverables
- [x] IDE IPC server lifecycle tied to project open and close
- [x] per-session socket creation in cache directory with restrictive permissions
- [x] worker-thread or queued request processing with no render loop blocking
- [x] `proto=1` JSON envelope request and response support
- [x] `idebridge ping` command end to end

phase completion criteria
- [x] CLI running inside IDE PTY can connect through `MYIDE_SOCKET`
- [x] `idebridge ping --json` returns valid structured response and exit code 0
- [x] invalid socket or no session returns predictable non-zero exit code

### phase_02_read_surface_mvp
status: [x]

deliverables
- [x] `idebridge diag --json [--max N]`
- [x] `idebridge symbols --json [--file path] [--top_level_only]`
- [x] `idebridge open path:line:col`
- [x] stable JSON schemas documented for diagnostics and symbols

phase completion criteria
- [x] diag output includes list plus severity summary counts
- [x] symbols output includes kind, name, file, range, and type or signature when available
- [x] open command is acknowledged by IDE and returns deterministic success or failure

### phase_03_high_leverage_queries
status: [x]

deliverables
- [x] `idebridge includes --json [--graph]`
- [x] `idebridge search --json <pattern> [--regex] [--files ...]`
- [x] `idebridge build --json [--profile debug|release]`

phase completion criteria
- [x] include graph payload is machine readable and consistent
- [x] search results contain file path, line, col, and matched text excerpt
- [x] build command reports exit code and summary payload predictably

### phase_04_patch_apply_pipeline
status: [x]

deliverables
- [x] `idebridge edit --apply <diff_file> [--no_hash_check]`
- [x] patch apply through IDE buffer pipeline, not direct unsafe file writes
- [x] hash check required by default
- [x] post-apply reindex and diagnostics refresh

phase completion criteria
- [x] undo or redo behavior remains intact after CLI patch apply
- [x] stale hash patch returns structured failure without partial corruption
- [x] response includes touched files and updated diagnostics summary

### phase_05_ux_and_scale
status: [x]

deliverables
- [x] robust timeout and error taxonomy for IPC failures
- [x] large output handling path, including optional file spill output mode
- [x] Linux path and permission compatibility pass
- [x] optional socket override support (`--socket`) and improved help output

phase completion criteria
- [x] large diag or symbol outputs remain valid and consumable by Codex
- [x] Linux smoke pass confirms socket lifecycle and command behavior
- [x] CLI help and error messaging are concise and actionable

### phase_06_hardening_and_release
status: [x]

deliverables
- [x] integration tests for request routing and critical commands
- [x] protocol compatibility policy documented
- [x] operational docs for running with multiple IDE instances
- [x] final walkthrough for Codex loop: diag -> patch -> apply -> verify

phase completion criteria
- [x] test suite covers protocol, connection, and key command regressions
- [x] docs fully describe command contracts and failure behavior
- [x] release checklist completed

## workflow
- [ ] draft a detailed checklist plan doc for the active phase
- [ ] execute tasks in order and mark checkboxes as each step completes
- [ ] when phase is fully complete, mark it complete in this file
- [ ] move that phase doc into `docs/codex_cli_upgrade/completed/`
- [ ] draft next phase plan and repeat
