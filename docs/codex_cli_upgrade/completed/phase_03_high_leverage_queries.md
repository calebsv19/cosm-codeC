# phase_03_high_leverage_queries

## goal
Add high-leverage read and execution commands so Codex can query include/dependency structure, run structured workspace search, and trigger canonical builds with machine-readable results.

## scope
- In scope: `includes`, `search`, `build` command surface and stable JSON payloads.
- Out of scope: patch apply and format tools.

## execution checklist
1. [x] add IDE IPC handler for `includes --json [--graph]`
2. [x] add IDE IPC handler for `search --json <pattern> [--regex] [--files ...]`
3. [x] add IDE IPC handler for `build --json [--profile debug|release]`
4. [x] wire includes payload to library index state with bucket metadata
5. [x] add graph edge payload for includes command when `--graph` is requested
6. [x] implement search matcher for literal and regex modes with deterministic limits
7. [x] add optional file filtering support for search command
8. [x] implement canonical build execution path with predictable exit code and output summary
9. [x] include diagnostics pointer or summary in build result payload
10. [x] implement `idebridge includes --json [--graph]`
11. [x] implement `idebridge search --json <pattern> [--regex] [--files ...]`
12. [x] implement `idebridge build --json [--profile debug|release]`
13. [x] add phase-3 runtime tests for includes/search/build success paths and key failures
14. [x] run build and targeted tests, then fix regressions
15. [x] update north star phase status and move this doc to `completed/`

## contracts

### includes result
- [x] include `buckets[]` with bucket kind, header name, resolved path, usage count
- [x] include usage entries with source path, line, column
- [x] include `summary` counts by bucket
- [x] when `graph=true`, include `edges[]` (source -> header/resolved)

### search result
- [x] include `pattern`, `regex` mode, and `match_count`
- [x] include matches with file path, line, column, and line excerpt
- [x] deterministic ordering by file then line then column

### build result
- [x] include `exit_code`, `ok`, and `status`
- [x] include command/working directory used
- [x] include bounded stdout or stderr summary
- [x] include diagnostics summary fields or pointer to `diag` response data

## test checklist
1. [x] includes json returns valid bucket schema
2. [x] includes graph mode returns edge list payload
3. [x] search literal mode returns expected matches
4. [x] search regex mode returns expected matches
5. [x] search file-filter mode restricts result set correctly
6. [x] build json returns deterministic success payload on valid project
7. [x] build json returns deterministic failure payload on failing build

## phase completion gate
- [x] all execution checklist items completed
- [x] all test checklist items completed
- [x] `docs/codex_cli_upgrade/north_star.md` phase_03 marked complete
- [x] this doc moved to `docs/codex_cli_upgrade/completed/`
