# phase_02_read_surface_mvp

## goal
Expose high-value IDE read and navigation commands to Codex via `idebridge` so Codex can query diagnostics/symbols and navigate files directly from terminal workflows.

## scope
- In scope: `diag`, `symbols`, `open`, response schema stability, basic tests.
- Out of scope: includes/search/build, patch apply.

## execution checklist
1. [x] add phase-2 command handlers in IDE IPC server for `diag`, `symbols`, and `open`
2. [x] define diagnostics JSON schema with summary counts and optional `max` truncation
3. [x] define symbols JSON schema with `--file` filtering and `--top_level_only` filtering
4. [x] add robust path normalization for absolute and project-relative file filters
5. [x] add IDE-side open request apply path that confirms cursor/file focus before ack
6. [x] implement `idebridge diag --json [--max N]`
7. [x] implement `idebridge symbols --json [--file path] [--top_level_only]`
8. [x] implement `idebridge open path:line:col`
9. [x] ensure stable non-zero exit behavior for server and protocol failures
10. [x] add targeted phase-2 tests for diag/symbols payloads and open success/failure contract
11. [x] run build and targeted tests, then fix regressions
12. [x] update north star phase status and move this doc to `completed/`

## contracts

### diag result
- [x] `summary.total`
- [x] `summary.error`
- [x] `summary.warn`
- [x] `summary.info`
- [x] `diagnostics[]` entries include: `file`, `line`, `col`, `endLine`, `endCol`, `severity`, `message`, `source`
- [x] include `total_count` and `returned_count`

### symbols result
- [x] `symbols[]` entries include: `kind`, `name`, `file`, `start_line`, `start_col`, `end_line`, `end_col`
- [x] include `signature` and `return_type` when available
- [x] include `owner` and `parent_kind` when available
- [x] include `total_count` and `returned_count`

### open result
- [x] accepts `path:line:col`
- [x] returns success only after IDE applies focus and cursor position
- [x] returns deterministic error code/message when open fails

## test checklist
1. [x] `idebridge diag --json` returns valid JSON envelope and summary fields
2. [x] `idebridge diag --json --max N` returns bounded count
3. [x] `idebridge symbols --json` returns valid symbols list schema
4. [x] `idebridge symbols --json --file path` filters correctly
5. [x] `idebridge symbols --json --top_level_only` filters to top-level/global symbols
6. [x] `idebridge open path:line:col` returns structured success on valid target
7. [x] `idebridge open path:line:col` returns structured failure on invalid target

## phase completion gate
- [x] all execution checklist items completed
- [x] all test checklist items completed
- [x] `docs/codex_cli_upgrade/north_star.md` phase_02 marked complete
- [x] this doc moved to `docs/codex_cli_upgrade/completed/`
