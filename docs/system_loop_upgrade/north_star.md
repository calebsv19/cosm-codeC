# System Loop Upgrade North Star

## Goal
Move the IDE from a poll-and-spin frame loop to an event-driven main loop that blocks when idle, wakes instantly on input/timers/worker completions, and keeps UI mutations on the main thread only.

## Why This Upgrade
Current behavior in `src/app/GlobalInfo/event_loop.c` and `src/app/main.c` still runs a continuous while-loop with per-iteration polling and soft `SDL_Delay(...)`. Even with dirty-frame gating, this keeps the loop hot at idle.

## Current State (What Exists)
- Dirty/invalidation render gate is already in place (`checkRenderFrame(...)`).
- Frame heartbeat exists (`IDE_RENDER_HEARTBEAT_MS`) to force periodic redraw.
- Main loop still spins continuously (`while (running) { runFrameLoop(...) }`).
- SDL events are drained via `SDL_PollEvent(...)` each iteration (non-blocking only).
- Background systems are ticked every loop:
  - `tickCommandBus()`
  - `pollFileWatcher()`
  - `terminal_tick_backend()`
  - `pollGitStatusWatcher()`
  - `analysis_scheduler_tick(...)`
- Analysis runs in a worker thread, but worker completion does not wake main via SDL user event.
- There is no unified deadline scheduler driving a single wait timeout.

## Target Architecture
Pipeline:
- SDL/input events + worker wake events + timer deadlines -> Main loop phases -> UI state updates -> render decision -> block until next wake.

Threading contract:
- Main thread: all UI/model mutation + Vulkan/SDL rendering.
- Workers: compute only; send result messages to main thread.

Wake contract:
- Main loop blocks using `SDL_WaitEventTimeout(...)`.
- Wake sources:
  - SDL input/window/drop events
  - SDL user wake event from workers
  - timeout for next scheduler deadline

## Success Criteria
- Idle CPU is near zero to low single-digit when no input/work is pending.
- Input responsiveness remains immediate (no perceivable sleep lag).
- Drag/resize/text interaction remains smooth under frame cap.
- Worker results are applied only on main thread via message queue.
- No UI regressions in rendering correctness, input focus, or pane updates.

## Phase Plan

### Phase 01 - Wake/Event Foundation
Status: `pending`

Objective:
- Introduce a dedicated wake event path before changing loop semantics.

Deliverables:
- Register `WAKE_EVENT` (`SDL_RegisterEvents(1)`) in startup.
- Add `mainthread_wake()` helper callable from workers/background systems.
- Add basic wake diagnostics counters (wake count, wake failures).
- Keep current loop behavior unchanged except plumbing.

Primary files:
- `src/app/GlobalInfo/system_control.c`
- `src/app/GlobalInfo/event_loop.c`
- new module under `src/core/` for wake API (or adjacent to scheduler)

### Phase 02 - Timer Deadline Scheduler
Status: `pending`

Objective:
- Replace ad-hoc per-loop periodic polling with explicit deadlines.

Deliverables:
- Main-thread timer scheduler module (one-shot + repeating timers, monotonic ms).
- API:
  - `scheduler_next_deadline_ms()`
  - `scheduler_fire_due(now_ms)`
- Migrate periodic systems to scheduler-driven timers:
  - watcher poll
  - git status poll
  - caret blink/debounce hooks where applicable

Primary files:
- new `src/core/...` scheduler module
- `src/core/Watcher/file_watcher.c`
- `src/ide/Panes/ToolPanels/Git/...`
- `src/app/GlobalInfo/event_loop.c`

### Phase 03 - Worker -> Main Message Queue
Status: `pending`

Objective:
- Standardize background result delivery and decouple worker completion from polling.

Deliverables:
- Thread-safe MPSC queue for worker messages.
- Message enum + payload ownership contract (owned payload + free path on main thread).
- Drain path in main loop phase.
- Analysis/Git/other workers push messages + call `mainthread_wake()`.
- Coalescing baseline:
  - git snapshot coalescing
  - progress throttling (e.g., 50-100ms)

Primary files:
- new queue/message module under `src/core/`
- `src/core/Analysis/analysis_job.c`
- `src/core/Analysis/analysis_scheduler.c`
- git worker/watch modules
- `src/app/GlobalInfo/event_loop.c`

### Phase 04 - Main Loop Blocking Rewrite
Status: `pending`

Objective:
- Convert to phased event loop with `SDL_WaitEventTimeout(...)` blocking.

Deliverables:
- New per-iteration phase order:
  1. drain SDL events
  2. drain worker messages
  3. fire due timers
  4. run main-thread queued jobs
  5. decide render
  6. compute next timeout
  7. block wait
- Define idle vs active-interaction policy:
  - active drag/resize/text-streaming => short waits/frame deadline
  - idle => long/infinite wait until event/deadline
- Remove soft spin pacing path as primary idle mechanism.

Primary files:
- `src/app/main.c`
- `src/app/GlobalInfo/event_loop.c`
- `src/core/InputManager/...` (interaction state signals)

### Phase 05 - Render/Interaction Coalescing
Status: `pending`

Objective:
- Keep responsiveness high while preventing wake/render thrash.

Deliverables:
- Motion/input coalescing (latest mouse position semantics per frame).
- Worker progress message throttle.
- Render cap application only when active rendering is needed.
- Preserve dirty-pane contract and avoid continuous redraw.

Primary files:
- `src/app/GlobalInfo/event_loop.c`
- `src/core/InputManager/input_mouse.c`
- worker message handling module

### Phase 06 - Verification, Diagnostics, and Hardening
Status: `pending`

Objective:
- Prove behavior and prevent regressions.

Deliverables:
- Runtime counters/overlay fields:
  - wake events/sec
  - queue depth
  - timers fired/sec
  - blocked time vs active time
- Acceptance checklist for:
  - idle CPU
  - drag smoothness
  - worker result latency
  - no duplicate/stale updates
- Fallback handling for rare wake push failures.

Primary files:
- diagnostics/timer HUD integration
- `src/app/GlobalInfo/event_loop.c`
- docs in `docs/system_loop_upgrade/phases/`

## Execution Rules
- Implement one phase at a time in order.
- Each phase gets a dedicated plan doc in `docs/system_loop_upgrade/phases/`.
- Move completed phase docs into `docs/system_loop_upgrade/completed/`.
- Update this North Star status after each phase is completed.

## Non-Goals
- No Vulkan renderer redesign.
- No UI mutation from worker threads.
- No premature lock-free complexity unless profiling proves necessity.
- No OS-native file watch migration in this track (timer poll is acceptable baseline).
