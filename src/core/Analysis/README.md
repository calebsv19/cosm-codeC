# Analysis Runtime

This subsystem owns workspace analysis, cache validation, and the persisted
metadata that powers diagnostics, symbols, tokens, and the Libraries panel.

| File Group | Responsibility |
| --- | --- |
| `analysis_scheduler.*` | Coalesces refresh requests from startup, workspace reloads, file watching, manual refreshes, project mutations, and library refresh actions; editor-save diagnostics can preempt active lower-priority workspace/index/symbol work while preserving saved-file hints and exposing pending-save state to UI status badges. |
| `analysis_job.*` | Owns the async worker lifecycle, slow-mode throttling, cancellation, forced full-refresh requests, and live active-buffer diagnostics jobs. |
| `analysis_snapshot.*` | Captures workspace file fingerprints so incremental runs can decide what actually changed. |
| `analysis_incremental_policy.*` | Centralizes saved-file hint filtering and include-graph fallback policy for incremental runs. |
| `analysis_artifact_io.*` | Provides IDE-local helpers for `<workspace>/ide_files` path construction, directory creation, size-capped text reads, and plain/atomic artifact writes. Store-specific JSON schemas stay in the owning store files. |
| `analysis_cache.*` | Saves and validates cache metadata, including build-args hashes, frontend fingerprints, and cached artifact files. |
| `analysis_cache_manifest.*` | Writes a bounded `ide_files/cache_manifest.json` trust report for startup and persisted-analysis artifact inspection without making the manifest source of truth; startup reports include the compact refresh-intent audit summary, and oversized optional token artifacts are reported/pruned. |
| `analysis_startup_audit.*` | Compares trusted cache metadata, current source fingerprints, cached snapshot coverage, and required artifact readiness into aggregate startup load state and refresh intent. |
| `analysis_status.*` | Exposes current analysis status, progress, cache presence, and last-error state to the rest of the IDE. |
| `analysis_runtime_events.*` | Emits deterministic runtime events for analysis store hydration/update handoff into the event-driven UI pipeline. |
| `analysis_store.*` | Stores per-file diagnostics and persists them to disk. |
| `analysis_symbols_store.*` | Stores extracted symbol data and persists it across sessions. |
| `analysis_token_store.*` | Stores token spans used by UI surfaces that need lexical data; persisted token JSON is bounded by the startup load cap, oversized writes remove stale output, and legacy oversized token caches are pruned on load. |
| `analysis_units_store.*` | Stores symbol-attached dimensional and concrete unit metadata from the `fisiCs` extension contract lane. |
| `analysis_build_graph_store.*` | Stores normalized summaries from `fisiCs.build_graph` source and manifest dry-run artifacts, including translation-unit and planned-action diagnostic status. |
| `analysis_memory_report_store.*` | Stores normalized summaries from `memory_check_report_v1` runtime sidecars, including summary counters and allocation leak sites. |
| `include_graph.*` | Tracks include relationships so dependent files can be invalidated correctly. |
| `include_path_resolver.*` | Resolves build flags/include paths and persists them alongside the analysis cache. Passive analysis keeps Makefile execution gated behind `IDE_TRUST_WORKSPACE_MAKE_VARS` and uses argv-based tool probes instead of shell commands. |
| `library_index.*` + `library_index_build.c` | Builds the library/include usage index shown in the Libraries panel. |
| `project_scan.*` | Performs full or incremental scans across the workspace plus one-file in-memory scans for live active-buffer diagnostics. |
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
  - Units attachments are consumed only when `FISICS_CONTRACT_CAP_EXTENSION_UNITS_ATTACHMENTS` is advertised; concrete unit identity fields are kept only when `FISICS_CONTRACT_CAP_EXTENSION_UNITS_CONCRETE` is also advertised.
- Build graph summaries are ingested through the separate `fisiCs.build_graph`
  artifact lane, not the frontend ABI. The IDE normalizes top-level,
  translation-unit, and planned-action `diagnostic_summary` state for IPC and
  cache consumers.
- Runtime memory-check reports are ingested through the separate
  `memory_check_report_v1` sidecar lane, not the frontend ABI and not compiler
  diagnostics. The IDE keeps summary counters and leak allocation sites in a
  dedicated report store.
