# Phase 01 - Scheduler and State Foundation

Status: `completed`
Owner: `IDE core/runtime`
Depends on: none

## Objective

Replace scattered refresh flag writes with one analysis scheduler and make analysis status/state transitions thread-safe and observable.

## Why This Phase First

- It removes the main source of duplicate runs.
- It gives a single place to enforce debounce/coalescing policy.
- It provides run-level telemetry needed for later phases.

## Scope

In-scope:

- New scheduler module for refresh requests.
- Migration of existing request call-sites to scheduler API.
- Run ID and reason tracking.
- Thread-safe status updates and snapshots.

Out-of-scope:

- Watcher algorithm redesign (Phase 2).
- Deep incremental rules rewrite (Phase 3).
- Scan pipeline merge (Phase 4).

## Proposed API

Add module:

- `src/core/Analysis/analysis_scheduler.h`
- `src/core/Analysis/analysis_scheduler.c`

Core types:

- `AnalysisRefreshReason`
- `AnalysisRefreshRequest`
- `AnalysisRunInfo`

Core functions:

- `analysis_scheduler_init(void)`
- `analysis_scheduler_request(AnalysisRefreshReason reason, bool force_full)`
- `analysis_scheduler_tick(const char* project_root, const char* build_args)`
- `analysis_scheduler_running(void)`
- `analysis_scheduler_snapshot(AnalysisRunInfo* out)`

## Reason Enum (initial)

- `ANALYSIS_REASON_STARTUP`
- `ANALYSIS_REASON_WORKSPACE_RELOAD`
- `ANALYSIS_REASON_WATCHER_CHANGE`
- `ANALYSIS_REASON_MANUAL_REFRESH`
- `ANALYSIS_REASON_PROJECT_MUTATION`
- `ANALYSIS_REASON_LIBRARY_PANEL_REFRESH`

## Coalescing Rules (Phase 1)

- If a run is active, additional requests are merged into one pending request.
- Pending request keeps:
  - `force_full = OR(all force_full requests)`
  - `reason_mask |= incoming_reason`
- On worker completion, scheduler starts exactly one follow-up run if pending exists.

## Thread-Safety Changes

Apply to analysis status/scheduler shared state:

- Protect mutable globals with `SDL_mutex` (or atomics where trivial).
- Ensure status snapshot reads are synchronized.
- Keep state transitions atomic from UI perspective.

## Files To Update

- `src/core/Analysis/analysis_status.c`
- `src/core/Analysis/analysis_status.h`
- `src/core/Analysis/analysis_job.c`
- `src/core/Analysis/analysis_job.h`
- `src/app/GlobalInfo/event_loop.c`
- `src/app/GlobalInfo/system_control.c`
- `src/ide/Panes/MenuBar/command_menu_bar.c`
- `src/ide/Panes/ToolPanels/Libraries/command_tool_libraries.c`
- Any direct `analysis_request_refresh()` call-sites discovered during implementation.

## Implementation Steps

1. [x] Add scheduler module and basic structs.
2. [x] Move run-start gate from `event_loop` into scheduler `tick`.
3. [x] Replace direct refresh requests with `analysis_scheduler_request(...)` at startup/workspace/library refresh paths.
4. [x] Add run ID (`uint64_t`) increment on each start.
5. [x] Attach reason mask + force_full state to each run.
6. [x] Add synchronized scheduler snapshot use in control/libraries/errors panels.
7. [x] Keep backward-compatible wrappers temporarily (`analysis_request_refresh`) routed to scheduler.
8. [x] Remove remaining redundant direct-flag writes and complete trigger reason coverage.

## Logging/Telemetry

Add concise lines:

- `[AnalysisRun #<id>] queued reason=<...> force_full=<0|1>`
- `[AnalysisRun #<id>] started reason=<...> mode=<full|incremental>`
- `[AnalysisRun #<id>] completed status=<ok|error> files=<n>`
- `[AnalysisRun #<id>] coalesced pending=<count>`

Default behavior:

- Keep per-file progress controlled by existing env toggle.
- Keep frontend semantic logs behind frontend-log toggle.

## Validation Checklist

- Workspace switch on large repo triggers one run, completes, and does not auto-loop.
- Manual refresh while running coalesces into one follow-up run.
- Repeated watcher events during active run do not create N runs.
- Status banner never stays in updating state after completion/failure.
- Libraries panel and control panel both reflect final fresh/error state.

## Rollback Plan

- Keep legacy request APIs as wrappers for one iteration.
- Guard scheduler adoption with compile-time switch if needed.
- If regressions appear, route wrappers back to legacy behavior quickly.

## Completion Criteria

- All refresh triggers go through scheduler API.
- Run ID + reason appears in logs.
- No duplicate startup/workspace analysis loops observed in test projects.
- Phase doc moved to `docs/analysis_upgrade/completed/` and north-star updated.
