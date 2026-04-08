# Analysis Runtime

This subsystem owns workspace analysis, cache validation, and the persisted
metadata that powers diagnostics, symbols, tokens, and the Libraries panel.

| File Group | Responsibility |
| --- | --- |
| `analysis_scheduler.*` | Coalesces refresh requests from startup, workspace reloads, file watching, manual refreshes, project mutations, and library refresh actions. |
| `analysis_job.*` | Owns the async worker lifecycle, slow-mode throttling, cancellation, and forced full-refresh requests. |
| `analysis_snapshot.*` | Captures workspace file fingerprints so incremental runs can decide what actually changed. |
| `analysis_cache.*` | Saves and validates cache metadata, including build-args hashes, frontend fingerprints, and cached artifact files. |
| `analysis_status.*` | Exposes current analysis status, progress, cache presence, and last-error state to the rest of the IDE. |
| `analysis_runtime_events.*` | Emits deterministic runtime events for analysis store hydration/update handoff into the event-driven UI pipeline. |
| `analysis_store.*` | Stores per-file diagnostics and persists them to disk. |
| `analysis_symbols_store.*` | Stores extracted symbol data and persists it across sessions. |
| `analysis_token_store.*` | Stores token spans used by UI surfaces that need lexical data. |
| `include_graph.*` | Tracks include relationships so dependent files can be invalidated correctly. |
| `include_path_resolver.*` | Resolves build flags/include paths and persists them alongside the analysis cache. |
| `library_index.*` + `library_index_build.c` | Builds the library/include usage index shown in the Libraries panel. |
| `project_scan.*` | Performs full or incremental scans across the workspace. |
| `fisics_bridge.*` | Bridges the IDE to the Fisics frontend for actual analysis passes. |
| `fisics_contract_validation.h` | Validates compiler contract id/version metadata and triggers degraded-mode safeguards when incompatible payloads are seen. |
| `fisics_frontend_guard.*` | Serializes access to the frontend so concurrent callers do not corrupt shared state. |

Artifacts are persisted under `<workspace>/ide_files/`. On startup the IDE
loads whatever still matches the current workspace/build fingerprint, then
schedules a fresh analysis pass to reconcile the cache with disk.

Contract boundary note:

- The IDE consumes `fisiCs` analysis data through a versioned contract lane (`fisiCs.analysis.contract`).
- When contract major compatibility fails, IDE analysis ingestion enters degraded mode:
  - diagnostics/includes are still consumed
  - symbols/tokens are dropped for safety
  - a warning is emitted
- For contract `1.2.x`, symbol graph ownership can include `parent_stable_id`:
  - when present, IDE should prefer it for parent/owner linkage
  - when missing, IDE stays in normal mode and logs a one-time fallback warning while using name/kind ownership matching
- For contract `1.3.x`, diagnostics include additive taxonomy identity metadata:
  - `severity_id` (stable enum lane: info/warning/error)
  - `category_id` (stable enum lane: analysis/parser/semantic/preprocessor/lexer/codegen/build)
  - `code_id` (stable numeric identity, mirrors diagnostic `code`)
  - when missing, IDE degrades safely to legacy `kind` + textual/category heuristics
- For contract `1.4.x`, producers advertise explicit `capabilities` flags:
  - IDE gates optional lanes (for example symbols/tokens) from advertised flags instead of relying only on inferred minor-version behavior.
  - Missing optional capabilities do not force full degraded mode; incompatible contract id/major still does.
