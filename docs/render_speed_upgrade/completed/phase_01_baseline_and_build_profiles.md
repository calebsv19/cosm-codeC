# Phase 01 Plan: Baseline + Build Profile Foundation

## Phase Goal
Create a trustworthy measurement and execution foundation so all future render optimization work is based on real bottlenecks, not debug-build noise.

## Why This Phase First
Current HUD data shows severe render cost, but the current build path uses sanitizer-heavy debug settings. Before deeper optimization, we need a clean `perf` profile and repeatable capture workflow.

## Scope
In scope:
- Build-system profile split (`debug` and `perf`).
- TimerHUD capture process standardization.
- Baseline metrics capture and doc record.
- Small hygiene fixes that affect measurement quality (log spam removal in hot path).

Out of scope:
- Deep text caching implementation.
- Dirty-flag architecture changes.
- Major renderer refactors.

## Deliverables
- `make perf` target (optimized, no sanitizers).
- `make debug` target (existing sanitizer behavior preserved).
- One baseline markdown report with measured values.
- Quick start commands for team reproducibility.
- Concise build-system overview doc: `docs/render_speed_upgrade/makefile_summary.md`.

## Implementation Checklist

### Unit A: Build Profile Split
Files:
- `makefile`

Tasks:
- [x] Add `BUILD_PROFILE ?= debug` selection.
- [x] Keep sanitizer flags in `debug` profile only.
- [x] Add optimized flags for `perf` profile (`-O2` or `-O3`, `-DNDEBUG`).
- [x] Ensure `perf` target selects perf profile cleanly.
- [x] Keep existing default behavior stable for current dev workflow.
- [x] Add `FISICS_SANITIZED` toggle to handle current sanitized `libfisics_frontend.a` dependency in perf profile.

Acceptance checks:
- [x] `make clean && make` still works as debug build.
- [x] `make clean && make perf` builds without sanitizer instrumentation.
- [x] Binary starts successfully in both modes.

### Unit B: Measurement Hygiene
Files:
- `src/app/GlobalInfo/event_loop.c`
- any obvious hot-path logging sites touched for measurement integrity

Tasks:
- [x] Remove or gate noisy per-event logging in hot paths (e.g., keydown spam).
- [x] Keep optional debug logging behind an env flag.

Acceptance checks:
- [x] No continuous console flood during normal typing.
- [x] Behavior remains debuggable when flag is enabled.

### Unit C: Baseline Capture Workflow
Files:
- `docs/render_speed_upgrade/baseline_report_template.md` (new)
- `docs/render_speed_upgrade/baseline_report_phase_01.md` (new)

Tasks:
- [x] Add template for consistent metric capture.
- [x] Define one fixed benchmark scenario:
  - Open same workspace.
  - Same window size.
  - Same panels visible.
  - 30-60 seconds of typical interaction (hover, scroll, edit).
- [ ] Record first baseline for `debug` and `perf`.
- [x] Record first baseline for `debug` and `perf`.

Acceptance checks:
- [x] Both profiles have captured numbers in markdown.
- [x] Report includes date, commit hash, and run conditions.

### Unit D: Phase Tracking Updates
Files:
- `docs/render_speed_upgrade/north_star.md`

Tasks:
- [x] Mark Phase 01 progress checkboxes as completed when done.
- [x] Add next-phase handoff notes for Phase 02.

Acceptance checks:
- [x] Phase 01 status is explicit.
- [x] Next step to Phase 02 is documented.

## Verification Plan
- [x] Build verification:
  - `make clean && make`
  - `make clean && make perf`
- [x] Runtime verification:
  - Launch IDE with TimerHUD enabled in both profiles.
  - Confirm HUD appears and logs are generated.
- [x] Metric verification:
  - Confirm `SystemLoop`, `RenderGate`, `RenderPipeline`, `Render`, `PaneRender`, `HudRender` captured in both profiles.

## Risks and Mitigations
- Risk: Perf profile diverges from daily dev behavior.
  - Mitigation: Keep debug as default and document explicit perf command.
- Risk: Inconsistent benchmark runs.
  - Mitigation: Lock fixed scenario and run notes in report template.
- Risk: Hidden logging still distorts results.
  - Mitigation: Search and gate remaining hot-loop logs before capture.

## Completion Gate
Mark Phase 01 complete only when all are true:
- [x] Build profile split merged and verified.
- [x] Measurement hygiene updates merged.
- [x] Baseline report committed with debug vs perf numbers.
- [x] `docs/render_speed_upgrade/north_star.md` updated with Phase 01 complete status.
- [x] Phase 02 kickoff note written.

## Current Notes
- `make debug` and `make perf` now pass.
- IDE now links explicit frontend archives by profile:
  - unsanitized: `../fisiCs/libfisics_frontend_unsanitized.a`
  - sanitized: `../fisiCs/libfisics_frontend_sanitized.a`
- `make perf` defaults to unsanitized frontend (`FISICS_SANITIZED ?= 0`).
- Convenience run targets are available: `make run-perf`, `make run-perf-sanitized`, `make run-debug`.

## Phase 02 Handoff Preview
Expected first targets in Phase 02:
- Replace expensive `getTextClampedLength` char-by-char width probing with faster strategy.
- Reduce transient text surface/texture creation in editor line rendering.
- Re-measure `PaneRender` and `Render` deltas after each unit.

## Completion Timestamp
- Completed on: 2026-02-20
