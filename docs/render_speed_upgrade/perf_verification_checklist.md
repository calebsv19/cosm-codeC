# Render Perf Verification Checklist

## Build + Run Mode
- [ ] `make perf -j4` passes.
- [ ] Use one of:
  - `make run-perf` (TimerHUD off, normal usage)
  - `make run-perf-log` (TimerHUD on, overlay off)
  - `make run-perf-hud` (TimerHUD on, overlay on)

## Runtime Scenarios
- [ ] Idle 20s (no interaction)
- [ ] Mouse hover sweep across panes
- [ ] Editor typing + scroll
- [ ] Resize drag (window and pane split)
- [ ] Rename popup flow (open/edit/confirm/cancel)
- [ ] File watcher refresh path
- [ ] Project drag overlay path

## TimerHUD Checks (`run-perf-log` preferred)
- [ ] `HudRender` near `0.0 ms` when overlay is disabled.
- [ ] `SystemLoop` remains stable (no sustained growth over time).
- [ ] `Input` spikes are brief and interaction-correlated.
- [ ] `BackgroundTick` remains low outside refresh/build activity.
- [ ] `RenderGate` varies with invalidation, not constant max pressure.

## Safety Toggles
- [ ] `IDE_FORCE_FULL_REDRAW=1` still works as rollback path.
- [ ] `IDE_TIMER_HUD=0` disables profiling/logging in normal runs.
- [ ] `IDE_TIMER_HUD_OVERLAY=0/1` reliably toggles overlay when profiling is enabled.

## Documentation Update
- [ ] Record date + command + summary metrics in phase doc.
- [ ] Note any regressions and whether they are blockers.
