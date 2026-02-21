# Phase 03 Plan: Dirty Scheduling and Loop Hygiene

## Phase Goal
Run layout/sync work only when state changes, so steady-state frames avoid unnecessary CPU work and jitter.

## Scope
In scope:
- Gate layout sync work in the main loop behind real layout state changes.
- Gate resize hit-zone rebuild behind geometry/visibility changes.
- Keep behavior stable for resize/split/panel visibility toggles.

Out of scope:
- Full event-driven render invalidation architecture.
- Large refactors of input subsystem.

## Implementation Checklist

### Unit A: Layout Sync Gating
Files:
- `src/app/GlobalInfo/event_loop.c`

Tasks:
- [x] Add layout-state snapshot (window size, layout dimensions, panel visibility).
- [x] Run `layout_static_panes` and editor bind only when snapshot changes.
- [x] Remove redundant popup sync call from frame loop layout pass.

Acceptance checks:
- [x] `make perf` compiles cleanly.
- [x] No regressions in panel toggle/resize behavior at runtime.

### Unit B: Resize Zone and Render-Layout Gating
Files:
- `src/engine/Render/render_pipeline.c`

Tasks:
- [x] Track geometry/visibility signature inside render pipeline.
- [x] Gate `layout_static_panes` call on signature changes.
- [x] Gate `updateResizeZones` on signature changes.

Acceptance checks:
- [x] `make perf` compiles cleanly.
- [x] Resize handles remain correct after window and panel layout changes.

### Unit C: Measurement + Validation
Files:
- `docs/render_speed_upgrade/north_star.md`
- `docs/render_speed_upgrade/baseline_report_phase_01.md` (append comparison note or link)

Tasks:
- [x] Capture new TimerHUD perf run and compare idle jitter.
- [x] Confirm `LayoutSync`, `ResizeLayout`, and `ResizeZones` stay near zero in steady-state.
- [x] Record results and decide whether additional dirty flags are needed in Phase 03.

Acceptance checks:
- [x] Timing evidence committed.
- [x] Phase 03 completion state clear in docs.

## Verification Plan
- [x] Build verification:
  - `make perf -j4`
- [x] Runtime verification:
  - `make run-perf`, then interact with resize, panel toggles, and normal editing.
- [x] Metric verification:
  - inspect `LayoutSync`, `ResizeLayout`, `ResizeZones`, and `SystemLoop` in TimerHUD logs.

## Current Status (This Pass)
- Phase 03 complete.
- Validation run (2026-02-20, commit d8264e3) showed:
  - `LayoutSync avg/max`: `0.0000 / 0.0000`
  - `ResizeLayout avg/max`: `0.0000 / 0.0000`
  - `ResizeZones avg/max`: `0.0000 / 0.0000`
  - `SystemLoop avg/max`: `54.163 / 56.954`
