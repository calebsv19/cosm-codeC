# Diagnostics

This directory contains the shared diagnostics state used by the IDE for
analysis and build feedback persistence.

| File | Responsibility |
| --- | --- |
| `diagnostics_engine.h/c` | Stores in-memory diagnostics, exposes add/query helpers, and persists analysis diagnostics to `workspace/ide_files/analysis_diagnostics.json`. |
| `diagnostic_explanations.h/c` | Stores the compiler diagnostic explanation catalog from `fisiCs` metadata or `--list-diagnostics --json`-shaped JSON and links diagnostics by stable `code_id` / `code_name`. |
| `diagnostic_context.h/c` | Stores optional compiler diagnostic JSON context (`include_stack`, `macro_trace`, `details`) and republishes it through analysis diagnostics when keys match. |

Keep this layer UI-agnostic so panes can render diagnostics without depending
on where they came from. Build-output-specific diagnostics live alongside the
build system and are merged at presentation time by callers such as IPC and
tool panels.
