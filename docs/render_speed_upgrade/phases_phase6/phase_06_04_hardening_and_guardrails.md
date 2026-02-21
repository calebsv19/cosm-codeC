# Phase 6.4 Plan: Hardening, Tuning, and Guardrails

## Goal
Finalize invalidation-driven rendering as stable default with regression protection and clear operating controls.

## Scope
In scope:
- Fix correctness edge cases.
- Tune invalidation granularity and heartbeat cadence.
- Add lightweight perf regression checks and docs.

Out of scope:
- New feature work unrelated to render invalidation model.

## Implementation Checklist

### Unit A: Correctness hardening
Files:
- touched phase 6 files across core/render/pane/input paths

Tasks:
- [x] Validate drag, popup, resize, split, theme changes, file watcher updates.
- [x] Add targeted fixes for stale cache/invalidation misses.
- [x] Keep fallback full redraw toggle available for emergency rollback.

### Unit B: Performance tuning
Files:
- `src/app/GlobalInfo/event_loop.c`
- `src/engine/Render/render_pipeline.c`

Tasks:
- [x] Tune heartbeat cadence and invalidation consumption rules.
- [x] Reduce noisy invalidation sources (hover spam, repeated identical state writes).
- [x] Confirm CPU and frame-time stability under long sessions.

### Unit C: Documentation and guardrails
Files:
- `docs/render_speed_upgrade/north_star_invalidation_rendering.md`
- `docs/render_speed_upgrade/baseline_report_phase_01.md` (or follow-up report)

Tasks:
- [x] Record before/after metrics for idle, typing, scrolling, resize.
- [x] Add "perf verification checklist" for future render changes.
- [x] Mark Phase 6 sub-phases complete and promote finalized defaults.

## Verification
- [x] `make perf -j4`
- [x] Runtime scenario checklist completed and documented.
- [x] TimerHUD shows sustained gains and no new spikes.

## Completion Gate
- Invalidation-driven render model is default and stable.
- Guardrails exist to catch regressions early.

## Progress Notes
- Added emergency rollback toggle: `IDE_FORCE_FULL_REDRAW=1`.
- Tuned default heartbeat interval to 500ms (still overrideable via `IDE_RENDER_HEARTBEAT_MS`).
- Added invalidation dedupe in `invalidatePane(...)` to avoid repeated identical dirty writes in the same frame.
- Runtime validation (user): split/resize/popup/filewatcher/rename flows reported working.
- Latest `run-perf-log` capture (`timerhud/ide/timing.json`, last 300 samples):
  - `SystemLoop avg/min/max`: `15.869 / 14.328 / 17.516`
  - `RenderGate avg/min/max`: `4.223 / 0.960 / 6.161`
  - `PaneRender avg/min/max`: `21.666 / 21.114 / 22.103`
  - `HudRender avg/min/max`: `0.000 / 0.000 / 0.000` (overlay disabled by design)
