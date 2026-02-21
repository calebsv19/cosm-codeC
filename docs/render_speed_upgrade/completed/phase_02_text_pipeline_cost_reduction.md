# Phase 02 Plan: Text Pipeline Cost Reduction

## Phase Goal
Reduce hot-path text measurement and transient render work so `PaneRender` and `Render` drop materially in perf builds.

## Scope
In scope:
- Faster text clamping logic in shared render text helpers.
- Safer/faster width measurement helper behavior for long strings.
- Small call-site wiring changes to avoid redundant helper work.
- Bounded text texture cache for repeated short UI labels.
- Build verification and phase status tracking.

Out of scope:
- Full glyph atlas implementation.
- Major renderer architecture refactors.

## Implementation Checklist

### Unit A: Replace Linear Clamp Probing
Files:
- `src/engine/Render/render_text_helpers.c`
- `src/engine/Render/render_text_helpers.h`
- `src/engine/Render/render_helpers.c`

Tasks:
- [x] Replace O(n) char-by-char clamp loop with bounded search.
- [x] Add font-aware clamp helper to avoid repeated active-font lookups per probe.
- [x] Update render helper call sites to use font-aware clamping path.

Acceptance checks:
- [x] Clamp logic compiles and runs without behavior regressions for empty/short/long labels.
- [x] `make perf` succeeds after clamp changes.

### Unit B: Width Helper Safety + Churn Reduction
Files:
- `src/engine/Render/render_text_helpers.c`
- `src/engine/Render/render_helpers.c`
- `src/engine/Render/render_helpers.h`
- `src/engine/Render/render_pipeline.c`

Tasks:
- [x] Remove fixed 1024-char truncation from width helper path.
- [x] Use stack buffer for common short strings and heap fallback for longer strings.
- [x] Add targeted micro-cache for repeated clamp queries.
- [x] Add bounded texture cache for repeated short text renders in draw hot paths.
- [x] Add cache shutdown hook in render pipeline teardown.

Acceptance checks:
- [x] No silent width truncation for long strings.
- [x] `make perf` succeeds after helper changes.
- [x] No compile regressions after texture cache integration.

### Unit C: Measurement Pass
Files:
- `docs/render_speed_upgrade/baseline_report_phase_01.md` (append deltas or link)
- `docs/render_speed_upgrade/north_star.md` (status note only)

Tasks:
- [x] Capture fresh perf numbers after Unit A/B (`IDE_TIMER_HUD=1`).
- [x] Compare `Render`/`PaneRender` against Phase 01 perf baseline.
- [x] Record deltas and decide whether cache sizing/tuning is needed.

Acceptance checks:
- [x] Updated timing evidence committed in docs.
- [x] Next sub-step decision (tune cache vs move to Phase 03) documented.

## Verification Plan
- [x] Build verification:
  - `make perf -j4`
- [x] Runtime verification:
  - run `make run-perf` with TimerHUD enabled and capture timing log.
- [x] Metric verification:
  - compare `Render` / `PaneRender` / `SystemLoop` against Phase 01 perf run.

## Current Status (This Pass)
- Completed Unit A and Unit B core work, including clamp + text texture cache.
- Phase 2 accepted based on user-reported TimerHUD improvement (down to ~50 ms range in active run).
- Next step moved to Phase 3 dirty scheduling and loop hygiene.
