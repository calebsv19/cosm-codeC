# Analysis Upgrade North Star

## Goal

Make workspace analysis deterministic, incremental, and observable so large projects do one clean pass on load, then only reprocess changed files.

## Success Criteria

- Workspace load triggers one scheduled analysis run (not repeated loops).
- Incremental refresh updates only dirty + dependent files when possible.
- UI status transitions are reliable (`idle -> stale/loading -> refreshing -> fresh/error`).
- Startup and refresh causes are visible and traceable in logs.
- Watcher and workspace reload behavior are debounced and coalesced.

## Current Problems To Eliminate

- Refresh requests come from multiple paths with direct flag mutation.
- Shared refresh/running state is vulnerable to race-like behavior.
- Watcher-triggered reloads can stack with manual/workspace refreshes.
- Duplicate scan logic increases maintenance risk.
- "Updating" UI can appear stuck when repeated runs are queued.

## Phase Plan

### Phase 1 - Scheduler + State Foundation

Status: `completed`

Create a single scheduler abstraction for refresh requests and move all call sites to it. Add run IDs + reason tracking and thread-safe status updates.

Deliverables:

- `analysis_scheduler` module with request API and coalescing.
- Unified reason enum (`workspace_reload`, `watcher_change`, `manual_refresh`, `project_mutation`, `startup`).
- Thread-safe analysis status reads/writes.
- Instrumented logs including run ID + reason.

### Phase 2 - Watcher + Trigger Hygiene

Status: `pending`

Refine watcher behavior so workspace switch and internal cache writes do not retrigger redundant refreshes.

Deliverables:

- Watcher suppression window for workspace switch.
- Internal-write ignore window while analysis persists `ide_files`.
- Debounce/cooldown policy documented and enforced in scheduler.

### Phase 3 - Incremental Pipeline Hardening

Status: `pending`

Make incremental behavior robust with explicit fallback rules and safer baseline checks.

Deliverables:

- Explicit baseline validity contract (snapshot + stores + index).
- Incremental target-set diagnostics (`dirty`, `removed`, `dependents`, `targets`).
- Guarded fallback to full scan with reason code.

### Phase 4 - Scan Pipeline Consolidation

Status: `pending`

Reduce duplicated scan paths and make outputs generated from one canonical parse pass where possible.

Deliverables:

- Shared scan/visit core for diagnostics, symbols, tokens, includes.
- Library-index update integrated into canonical pass (or clear adapter boundary if split retained).
- Reduced duplicate traversal code.

### Phase 5 - UX + Operability

Status: `pending`

Improve visibility and operator controls so long runs feel responsive and debuggable.

Deliverables:

- Structured progress stages (`discover`, `analyze`, `index`, `persist`).
- Quiet/verbose frontend logging toggle (runtime + persisted preference optional).
- Analysis counters surfaced for validation (queued/started/completed/coalesced).

## Execution Model

- Work one phase at a time.
- Create a detailed per-phase doc in `docs/analysis_upgrade/phases/`.
- On completion, move phase doc to `docs/analysis_upgrade/completed/`.
- Update this north-star status section after each completed phase.
