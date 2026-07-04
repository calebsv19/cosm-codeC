# Diagnostics

This directory contains the shared diagnostics state used by the IDE for
analysis and build feedback persistence.

| File | Responsibility |
| --- | --- |
| `diagnostics_engine.h/c` | Stores in-memory diagnostics, exposes add/query helpers, persists analysis diagnostics to `workspace/ide_files/analysis_diagnostics.json`, and reports deterministic artifact IO status for save/load routes. |
| `diagnostic_explanations.h/c` | Stores the compiler diagnostic explanation catalog from `fisiCs` metadata or `--list-diagnostics --json`-shaped JSON and links diagnostics by stable `code_id` / `code_name`. |
| `diagnostic_context.h/c` | Stores optional compiler diagnostic JSON context (`include_stack`, `macro_trace`, `details`) and republishes it through analysis diagnostics when keys match. |

Keep this layer UI-agnostic so panes can render diagnostics without depending
on where they came from. Build-output-specific diagnostics live alongside the
build system and are merged at presentation time by callers such as IPC and
tool panels.

## Artifact IO Reporting

`diagnostics_save_report(...)` and `diagnostics_load_report(...)` preserve the
legacy artifact path while returning a `DiagnosticIoReport` with operation,
reason, path, row counts, malformed-row count, and byte size. First-run missing
diagnostics files report `missing_ok` without noisy stderr. Abnormal artifact
failures such as invalid JSON, invalid root shape, oversized files, malformed
rows, failed saves, or invalid workspace input emit one bounded
`[DiagnosticsIO]` stderr line and expose the reason for tests or future status
surfaces.
