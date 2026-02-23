# Phase 02 - Watcher and Trigger Hygiene

Status: `in_progress`
Owner: `IDE core/runtime`
Depends on: `completed/phase_01_scheduler_and_state_foundation.md`

## Objective

Prevent redundant analysis reruns caused by noisy file-system signals and internal IDE writes, while preserving fast refresh when user/project files actually change.

## Why This Phase

- Current scheduler coalescing helps, but watcher noise can still repeatedly queue refresh.
- Internal analysis persistence (`ide_files/*`) and short-lived workspace churn can look like "real" project updates.
- Large projects need stricter trigger hygiene to avoid apparent loops/hangs.

## Scope

In-scope:

- Watcher debounce/cooldown policy for workspace-level refresh requests.
- Internal-write ignore window during/after analysis persistence.
- Trigger dedupe so identical stamps/signals do not requeue immediately.
- Structured watcher diagnostics behind env flag.

Out-of-scope:

- Incremental dependency algorithm changes (Phase 3).
- Scan pipeline consolidation (Phase 4).

## Current State Summary

- Workspace watcher uses coarse top-level mtime stamp polling.
- Workspace switch suppression exists, but only as a fixed initial window.
- Refresh requests can still recur when stamp changes cluster over short intervals.
- Scheduler coalesces requests but still receives noisy enqueue traffic.

## Design

### 1) Debounce + Cooldown Gates

Add watcher-side gates:

- `min_trigger_gap_ms`: minimum time between accepted workspace refresh triggers.
- `debounce_window_ms`: stamp change must persist long enough before triggering.

Behavior:

- First observed stamp change starts debounce timer.
- If stamp remains changed through debounce window and cooldown allows, queue one refresh.
- Reset pending debounce state after trigger.

### 2) Internal Write Suppression

Add API to watcher:

- `watcher_suppress_internal_refresh_for_ms(duration)`

Used by analysis lifecycle:

- Called when analysis run starts and near persist completion.
- Prevents `ide_files`/cache write bursts from causing workspace refresh queue churn.

### 3) Trigger Reason Hygiene

- Ensure watcher-origin refreshes always queue with `ANALYSIS_REASON_WATCHER_CHANGE`.
- Ensure workspace/manual paths do not double-queue watcher reason during suppression windows.

### 4) Diagnostics

Behind `IDE_FILE_WATCHER_LOG=1`, print concise lines:

- stamp observed
- debounce started/elapsed
- trigger accepted/suppressed (with reason: cooldown/debounce/internal-suppress)

## Files To Update

- `src/core/Watcher/file_watcher.h`
- `src/core/Watcher/file_watcher.c`
- `src/core/Analysis/analysis_scheduler.c` (hook internal suppression around active run)
- `src/app/GlobalInfo/event_loop.c` (only if small plumbing needed)

## Implementation Steps

1. [x] Add watcher state fields for debounce/cooldown/internal suppression.
2. [x] Implement debounce evaluation in `pollFileWatcher()`.
3. [x] Implement cooldown enforcement after successful watcher-triggered queue.
4. [x] Add public internal suppression function and call-sites from analysis lifecycle.
5. [x] Add watcher debug logs (env-gated).
6. [ ] Validate with large workspace: no repeated 65-file loops; one run per real change burst.

## Validation Checklist

- Workspace switch triggers a single reload/analysis run.
- No repeated automatic reruns when analysis is actively persisting cache files.
- Touching one source file causes one watcher-triggered refresh.
- Rapid burst writes coalesce into one watcher refresh (within debounce window).
- UI "Updating..." returns to fresh state without immediate re-entry loops.

## Rollback Plan

- Keep debounce/cooldown defaults conservative and env-overridable.
- If regressions appear, gate new logic with env flag to disable and fall back quickly.

## Completion Criteria

- Watcher trigger hygiene mechanisms merged and enabled by default.
- Large-project reopen no longer loops repeated full analysis passes.
- Phase doc moved to `docs/analysis_upgrade/completed/` and north-star updated.
