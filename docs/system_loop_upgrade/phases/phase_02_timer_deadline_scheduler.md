# Phase 02 Plan: Timer Deadline Scheduler

## Goal
Introduce a single main-thread timer scheduler and migrate periodic watcher polling to scheduled deadlines instead of unconditional per-loop polling.

## Scope
In scope:
- Add a main-thread timer scheduler module (one-shot + repeating).
- Add next-deadline query and due-fire APIs.
- Move file watcher and git status watcher polling to repeating scheduled timers.
- Keep existing behavior/stability while reducing always-on loop work.

Out of scope:
- `SDL_WaitEventTimeout(...)` blocking rewrite (Phase 04).
- Worker message queue redesign (Phase 03).

## Implementation Checklist

### Unit A: Scheduler module
Files:
- `src/core/LoopTimer/mainthread_timer_scheduler.h`
- `src/core/LoopTimer/mainthread_timer_scheduler.c`

Tasks:
- [x] Add init/shutdown/reset.
- [x] Add one-shot and repeating timer registration.
- [x] Add cancel API.
- [x] Add `scheduler_next_deadline_ms()` query.
- [x] Add `scheduler_fire_due(now_ms)` processing.

### Unit B: Watcher interval integration
Files:
- `src/core/Watcher/file_watcher.h`
- `src/core/Watcher/file_watcher.c`

Tasks:
- [x] Expose watcher poll interval getter for scheduler registration.
- [x] Remove internal self-throttle that duplicates scheduler cadence.
- [x] Preserve debounce/cooldown semantics for workspace refresh behavior.

### Unit C: Git watcher interval integration
Files:
- `src/ide/Panes/ToolPanels/Git/tool_git.h`
- `src/ide/Panes/ToolPanels/Git/tool_git.c`

Tasks:
- [x] Expose git watcher poll interval getter for scheduler registration.
- [x] Remove internal self-throttle that duplicates scheduler cadence.
- [x] Preserve hash/coalescing behavior.

### Unit D: Event loop wiring
Files:
- `src/app/GlobalInfo/system_control.c`
- `src/app/GlobalInfo/event_loop.c`

Tasks:
- [x] Initialize/shutdown timer scheduler.
- [x] Register repeating timers (watcher + git watcher).
- [x] Replace direct per-loop watcher/git polling with `scheduler_fire_due(now_ms)`.
- [x] Keep the rest of frame loop behavior unchanged.

## Verification
- [x] Build passes (`make`).
- [ ] File watcher still detects external changes.
- [ ] Git panel still refreshes on repo changes.
- [ ] No regressions in normal editing/render behavior.

## Completion Notes
- Added scheduler module:
  - `src/core/LoopTimer/mainthread_timer_scheduler.h`
  - `src/core/LoopTimer/mainthread_timer_scheduler.c`
- Scheduler now initialized/shutdown via `system_control.c`.
- Event loop now registers repeating timers once and drives them via:
  - `mainthread_timer_scheduler_fire_due(SDL_GetTicks())`
- Migrated watcher cadence ownership to scheduler:
  - `pollFileWatcher()` no longer self-throttles.
  - `pollGitStatusWatcher()` no longer self-throttles.
- Exposed interval getters:
  - `fileWatcherPollIntervalMs()`
  - `gitStatusWatchIntervalMs()` (`IDE_GIT_WATCHER_POLL_MS` supported).
- Compile verification passed with `make -j4`.
- Remaining items are runtime/manual validation in IDE.
