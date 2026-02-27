# Diagnostics

This directory contains the shared diagnostics state used by the IDE for
analysis and build feedback persistence.

| File | Responsibility |
| --- | --- |
| `diagnostics_engine.h/c` | Stores in-memory diagnostics, exposes add/query helpers, and persists analysis diagnostics to `workspace/ide_files/analysis_diagnostics.json`. |

Keep this layer UI-agnostic so panes can render diagnostics without depending
on where they came from. Build-output-specific diagnostics live alongside the
build system and are merged at presentation time by callers such as IPC and
tool panels.
