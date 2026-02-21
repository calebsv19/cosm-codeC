# Render Speed Upgrade North Star

## Goal
Make IDE interaction feel consistently smooth and responsive by cutting frame time variance and reducing worst-case render cost in the main loop.

## Current Baseline (from TimerHUD capture)
- `SystemLoop`: ~116 ms
- `RenderGate`: ~114 ms
- `RenderPipeline`: ~114 ms
- `Render`: ~108 ms
- `PaneRender`: ~108 ms
- `HudRender`: ~6 ms

Interpretation: the IDE is currently render-bound. Most optimization effort should target editor/terminal/UI draw paths and text rendering behavior.

## Phase Roadmap
1. Phase 01: Establish reliable perf baselines + build profile split.
2. Phase 02: Remove high-cost per-frame text work in editor and shared text helpers.
3. Phase 03: Add dirty-layout/dirty-render scheduling and cut unconditional loop work.
4. Phase 04: Caching + batching for text and UI draw paths.
5. Phase 05: Background system throttling and frame pacing stabilization.
6. Phase 06: Hardening, regression gates, and continuous perf guardrails.

## Phase Details

### Phase 01: Baseline + Build Profile Foundation
Objective:
- Ensure measurements represent real runtime behavior.
- Separate dev-safety builds from performance builds.

Primary deliverables:
- `perf` build target without sanitizers and with optimization flags.
- A repeatable benchmark scenario and capture checklist.
- A baseline report in docs with recorded timing metrics.

Completion criteria:
- Perf build exists and is documented.
- Baseline capture run is repeatable and checked in as markdown notes.
- Team can compare `debug` vs `perf` numbers quickly.

### Phase 02: Text Pipeline Cost Reduction
Objective:
- Eliminate expensive per-line/per-character text measurement and transient text uploads in hot render paths.

Primary deliverables:
- Replace linear char-by-char clipping measurements with bounded/binary strategies.
- Cache measured widths for repeated strings/line slices.
- Reduce texture/surface churn for repeated text draws.

Completion criteria:
- `PaneRender` time is materially reduced in perf runs.
- Editor scrolling/hovering no longer causes major frame spikes.

### Phase 03: Dirty Scheduling and Loop Hygiene
Objective:
- Only run layout/theme/pane sync work when state changes.

Primary deliverables:
- Dirty flags for layout/theme/editor hitboxes.
- Remove recursive hitbox rebuild from recursive layout.
- Avoid unconditional full pane relayout in steady-state frames.

Completion criteria:
- `LayoutSync` stays near zero during idle interaction.
- No functional regressions in resize/split/input behavior.

### Phase 04: Render Cache + Batching Layer
Objective:
- Introduce cache-aware rendering for repeated UI text and stable widget geometry.

Primary deliverables:
- Shared text atlas or glyph/label cache policy.
- Batched draw submission strategy for repeated UI elements.
- Cache invalidation rules tied to dirty states.

Completion criteria:
- Reduced draw-call and upload churn in capture logs.
- Stable frame times during normal editing and panel interaction.

### Phase 05: Background Work Throttling + Pacing
Objective:
- Reduce non-render work stealing frame budget.

Primary deliverables:
- Throttled watcher polling interval.
- Remove hot-path debug prints in input/render loops.
- Smoother frame pacing policy under mixed input + background load.

Completion criteria:
- Input latency is consistent under file watcher/analysis activity.
- `SystemLoop` jitter is reduced in TimerHUD logs.

### Phase 06: Hardening + Perf Guardrails
Objective:
- Keep performance wins from regressing.

Primary deliverables:
- Lightweight perf checklist in PR/release flow.
- Repeatable perf scenario commands.
- Doc update and known-risk tracking.

Completion criteria:
- Perf checklist is part of team workflow.
- Baseline and current runs are comparable and stored in docs.

## Working Process
- Active phase plans live in `docs/render_speed_upgrade/phases/`.
- Completed phase plans move to `docs/render_speed_upgrade/completed/`.
- Only one phase is active at a time.
- Each phase doc must include:
  - Scope and non-goals
  - Concrete implementation checklist
  - Verification steps
  - Completion gate

## Current Status
- Phase 01: completed on 2026-02-20 (`docs/render_speed_upgrade/completed/phase_01_baseline_and_build_profiles.md`)
- Phase 02: completed (`docs/render_speed_upgrade/completed/phase_02_text_pipeline_cost_reduction.md`)
- Phase 03: completed (`docs/render_speed_upgrade/completed/phase_03_dirty_scheduling_and_loop_hygiene.md`)
- Phase 04: completed (`docs/render_speed_upgrade/completed/phase_04_render_cache_and_batching.md`)
- Active phase: Phase 05 (`docs/render_speed_upgrade/phases/phase_05_background_throttling_and_pacing.md`)
- Phase 06: pending

## Next Main Plan
- New main plan for the invalidation-driven render model:
  `docs/render_speed_upgrade/north_star_invalidation_rendering.md`
- Sub-phases for this plan live in:
  `docs/render_speed_upgrade/phases_phase6/`
