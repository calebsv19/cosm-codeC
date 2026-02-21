# Phase 01 Baseline Report

## Benchmark Scenario (Fixed)
- Workspace path: same project for all runs
- Window size: `1600x860`
- Visible panes: default IDE layout (menu/icon/tool/editor/control/terminal)
- Interaction script (30-60s):
  - 10s mouse hover across panes
  - 15s editor vertical scroll + cursor movement
  - 15s typing/editing in a medium file
  - 10s terminal pane interaction

## Run Commands
- Debug build:
  - `make clean && make debug -j4`
  - `IDE_TIMER_HUD=1 ./ide`
- Perf build:
  - `make clean && make perf -j4`
  - `IDE_TIMER_HUD=1 ./ide`

## Capture Template

### Debug Run
- Date: 2026-02-20
- Commit: d8264e3
- Notes: Baseline from pre-perf-build TimerHUD capture (HUD screenshot/log snapshot before unsanitized perf split).
- Metrics:
  - SystemLoop: 116.74 ms
  - RenderGate: 114.34 ms
  - RenderPipeline: 114.34 ms
  - Render: 108.07 ms
  - PaneRender: 107.80 ms
  - HudRender: 5.95 ms
  - Present: 0.33 ms

### Perf Run
- Date: 2026-02-20
- Commit: d8264e3
- Notes: Unsanitized perf build (`make perf`, default `FISICS_SANITIZED=0`), values from `timerhud/ide/timing.json`.
- Metrics:
  - SystemLoop: 99.10 ms
  - RenderGate: 96.14 ms
  - RenderPipeline: 97.34 ms
  - Render: 90.44 ms
  - PaneRender: 90.33 ms
  - HudRender: 5.10 ms
  - Present: 1.80 ms

## Comparison Summary
- Biggest delta: `SystemLoop` improved by ~17.64 ms and `PaneRender` improved by ~17.47 ms.
- Remaining top bottleneck: render path (`Render`/`PaneRender`) still dominates frame time.
- Phase 02 target confirmation: proceed with text-path cost reduction and transient text churn cuts.

## Phase 03 Validation Note
- Date: 2026-02-20
- Commit: d8264e3
- Source: `timerhud/ide/timing.json` (last 120 samples after resize/toggle interaction)
- Results:
  - `LayoutSync avg/max`: `0.0000 / 0.0000`
  - `ResizeLayout avg/max`: `0.0000 / 0.0000`
  - `ResizeZones avg/max`: `0.0000 / 0.0000`
  - `SystemLoop avg/max`: `54.163 / 56.954`
- Conclusion: Phase 03 dirty scheduling behavior matches expected outcome.

## Phase 6.4 Validation Note
- Date: 2026-02-21
- Command: `make run-perf-log`
- Source: `timerhud/ide/timing.json` (last 300 samples)
- Results:
  - `SystemLoop avg/min/max`: `15.869 / 14.328 / 17.516`
  - `RenderGate avg/min/max`: `4.223 / 0.960 / 6.161`
  - `PaneRender avg/min/max`: `21.666 / 21.114 / 22.103`
  - `HudRender avg/min/max`: `0.000 / 0.000 / 0.000` (overlay intentionally disabled)
- Runtime checklist: split/resize/popup/filewatcher/rename flows validated.
- Conclusion: Invalidation-driven path is stable and materially faster than the original baseline class; Phase 6 hardened defaults accepted.
