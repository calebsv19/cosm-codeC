# Phase 6.1 Plan: Invalidation Contract and State Plumbing

## Goal
Define a single invalidation model that can express "what changed" and "what must redraw" without changing rendering behavior yet.

## Scope
In scope:
- Add per-pane dirty state and dirty-reason bits.
- Add global frame invalidation state in core state.
- Add helper APIs for `invalidate_pane(...)`, `invalidate_all(...)`, `consume_invalidation(...)`.
- Add instrumentation counters for invalidation reason frequency.

Out of scope:
- Pane texture caching.
- Behavior changes to draw ordering.
- Removing full redraw path.

## Implementation Checklist

### Unit A: Per-pane metadata
Files:
- `src/ide/Panes/PaneInfo/pane.h`
- `src/ide/Panes/PaneInfo/pane.c` (or creation helpers)

Tasks:
- [x] Add dirty flags/bitmask to `UIPane`.
- [x] Add `lastRenderFrameId` and optional dirty region rect (start coarse if needed).
- [x] Initialize defaults in pane creation paths.

### Unit B: Global invalidation state
Files:
- `src/app/GlobalInfo/core_state.h`
- `src/app/GlobalInfo/core_state.c`

Tasks:
- [x] Add `frameInvalidated`, `fullRedrawRequired`, `frameCounter`.
- [x] Add bitmask enum for invalidation reasons.
- [x] Add API helpers to set/clear/query invalidation state.

### Unit C: Trigger wiring (no render behavior change yet)
Files:
- `src/app/GlobalInfo/event_loop.c`
- input/layout/theme paths that already detect state changes

Tasks:
- [x] On relevant events, set pane/global invalidation reason bits.
- [x] Keep old rendering path intact; only collect state + counters.
- [x] Add optional debug log env flag: `IDE_INVALIDATION_LOG=1`.

## Verification
- [x] `make perf -j4`
- [x] `make run-perf` still renders with no regressions.
- [x] TimerHUD unchanged or near-unchanged (expected for this phase).
- [x] Invalidation logs show correct reasons during: typing, hover, resize, pane toggle.

## Completion Gate
- Dirty/invalidation API exists and is used in key event paths.
- No functional regressions.
- Ready to switch render gate logic in Phase 6.2.

## Status
- Completed.
