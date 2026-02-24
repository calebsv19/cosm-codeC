# Phase 01 Plan: Wake Event Foundation

## Goal
Add a safe, reusable main-thread wake mechanism using SDL user events so workers can wake the UI loop without direct UI mutation.

## Scope
In scope:
- Register a dedicated SDL user event (`WAKE_EVENT`).
- Provide a small wake API callable from any thread.
- Track basic wake diagnostics counters.
- Consume wake events in the event loop without heavy work.
- Wire at least one worker path to push wake events.

Out of scope:
- Blocking loop rewrite (`SDL_WaitEventTimeout`).
- Timer/deadline scheduler.
- Worker message queue redesign.

## Implementation Checklist

### Unit A: Wake API module
Files:
- `src/core/LoopWake/mainthread_wake.h`
- `src/core/LoopWake/mainthread_wake.c`

Tasks:
- [x] Add `mainthread_wake_init()` and `mainthread_wake_shutdown()`.
- [x] Add `mainthread_wake_push()` helper for worker threads.
- [x] Add `mainthread_wake_is_event(...)` and receive accounting.
- [x] Add diagnostics snapshot struct + read API.

### Unit B: Startup/shutdown integration
Files:
- `src/app/GlobalInfo/system_control.c`

Tasks:
- [x] Initialize wake system during startup after SDL init.
- [x] Shutdown wake system during system teardown.
- [x] Log warning if wake registration fails.

### Unit C: Event loop integration
Files:
- `src/app/GlobalInfo/event_loop.c`

Tasks:
- [x] Detect and consume wake events in `processInputEvents`.
- [x] Ensure wake events do not route through normal input handlers.
- [x] Keep current polling loop behavior unchanged.

### Unit D: Worker wiring baseline
Files:
- `src/core/Analysis/analysis_job.c`

Tasks:
- [x] Push wake event when analysis worker exits (success/cancel/error paths).
- [x] Push wake on worker start failure path where useful.

## Verification
- [x] Build passes (`make`).
- [ ] IDE runs with no input regressions.
- [ ] Analysis completion triggers at least one wake event reception (debug counters/log validation).

## Completion Notes
- Implemented new wake module:
  - `src/core/LoopWake/mainthread_wake.h`
  - `src/core/LoopWake/mainthread_wake.c`
- Integrated wake init/shutdown and startup warning in `system_control.c`.
- Wake events are consumed in `processInputEvents(...)` before normal input routing.
- Analysis worker now pushes wake events on completion/cancel/no-root/start-failure paths.
- Verified compile success with `make -j4`.
- Remaining validation is runtime/manual (interaction + wake counter observation).
