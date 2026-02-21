# Phase 04 Plan: Render Cache and Batching

## Phase Goal
Stabilize render cost by improving cache hit effectiveness and reducing avoidable draw/upload churn in steady-state interaction.

## Scope
In scope:
- Upgrade text cache policy with explicit memory budget and LRU eviction.
- Expand cache usefulness for real editor/UI line lengths.
- Add lifecycle safety for cache invalidation on renderer changes.
- Measure cache-policy impact with TimerHUD.

Out of scope:
- Full glyph atlas implementation.
- New renderer backend architecture.

## Implementation Checklist

### Unit A: Cache Policy Hardening
Files:
- `src/engine/Render/render_helpers.c`
- `src/engine/Render/render_pipeline.c`

Tasks:
- [x] Increase cache eligibility for longer text lines.
- [x] Add global cache memory budget.
- [x] Add global LRU eviction policy.
- [x] Invalidate cache when renderer instance changes.

Acceptance checks:
- [x] `make perf` compiles and links.
- [x] Runtime sanity check: no crashes/artifacts after repeated run/restart/resize.

### Unit B: Cache Effectiveness Validation
Files:
- `timerhud/ide/timing.json` (runtime evidence)
- `docs/render_speed_upgrade/baseline_report_phase_01.md` (append phase note or link)

Tasks:
- [x] Capture perf run with typical editing + scrolling.
- [x] Compare `Render` and `PaneRender` against post-Phase-3 behavior.
- [x] Record whether cache hit profile appears stable (no worsening over session time).

Acceptance checks:
- [x] Metrics recorded in docs.
- [x] Decision logged: continue tuning Phase 4 vs move to Phase 5.

## Verification Plan
- [x] `make perf -j4`
- [x] `make run-perf` with TimerHUD enabled.
- [x] Review latest TimerHUD log tail after 30-60s interaction.

## Current Status
- Phase 04 complete.
- Observed steady-state run near ~50 ms system loop with render path around ~41-43 ms and stable cache behavior.
