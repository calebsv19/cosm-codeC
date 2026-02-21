# Phase 05 Plan: Background Throttling and Frame Pacing

## Phase Goal
Reduce non-render frame cost and jitter by throttling background polling and improving idle frame pacing behavior.

## Scope
In scope:
- Throttle expensive background watchers that do not need per-frame polling.
- Remove avoidable hot-path log spam in watcher loops.
- Add soft pacing yield when the loop is ahead of the next render frame.
- Validate `SystemLoop` jitter behavior with TimerHUD.

Out of scope:
- Major scheduler rewrite.
- Async job system redesign.

## Implementation Checklist

### Unit A: Background Poll Throttling
Files:
- `src/core/Watcher/file_watcher.c`

Tasks:
- [x] Add poll interval throttling to file watcher loop.
- [x] Add env override for poll interval (`IDE_FILE_WATCHER_POLL_MS`).
- [x] Gate file watcher console spam behind env flag (`IDE_FILE_WATCHER_LOG`).

Acceptance checks:
- [x] `make perf` compiles cleanly.
- [ ] Runtime: file change detection still works with throttled polling.

### Unit B: Soft Frame Pacing
Files:
- `src/app/GlobalInfo/event_loop.c`

Tasks:
- [x] Return render/no-render state from render gate helper.
- [x] Add bounded `SDL_Delay` yield when ahead of target frame time.

Acceptance checks:
- [x] `make perf` compiles cleanly.
- [ ] Runtime: no input lag regressions from pacing delay.

### Unit C: Validation and Tuning
Files:
- `timerhud/ide/timing.json`
- `docs/render_speed_upgrade/baseline_report_phase_01.md` (append note or link)

Tasks:
- [ ] Capture TimerHUD run after Phase 5 changes.
- [ ] Compare `SystemLoop`, `Input`, `BackgroundTick` avg/max against pre-Phase-5 run.
- [ ] Tune watcher poll interval default if needed.

Acceptance checks:
- [ ] Jitter profile documented.
- [ ] Phase 05 completion status explicit in docs.

## Verification Plan
- [x] `make perf -j4`
- [ ] `make run-perf` with TimerHUD enabled.
- [ ] Validate file watcher behavior by editing files externally.

## Current Status
- Unit A and Unit B code paths implemented.
- Remaining work is runtime validation and timer-based tuning.
