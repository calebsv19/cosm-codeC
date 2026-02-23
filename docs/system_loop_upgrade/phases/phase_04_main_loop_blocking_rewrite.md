# Phase 04 Plan: Main Loop Blocking Rewrite

## Goal
Convert the loop from always-spinning pacing to timeout-based blocking using `SDL_WaitEventTimeout(...)` while preserving interaction responsiveness.

## Scope
In scope:
- Add phased wait decision logic at end of loop iteration.
- Use `SDL_WaitEventTimeout(...)` when idle-ish.
- Keep active interaction paths responsive (short waits / frame deadline).
- Integrate timer scheduler deadlines into wait timeout selection.
- Keep existing rendering pipeline and invalidation model.

Out of scope:
- Full queue migration of every background subsystem.
- Renderer backend redesign.

## Implementation Checklist

### Unit A: Event handling refactor
Files:
- `src/app/GlobalInfo/event_loop.c`

Tasks:
- [x] Refactor event processing so a pre-waited event can be processed first.
- [x] Keep wake-event consumption behavior intact.
- [x] Preserve current input invalidation behavior.

### Unit B: Wait timeout policy
Files:
- `src/app/GlobalInfo/event_loop.c`

Tasks:
- [x] Add timeout computation from:
  - frame deadline (when invalidated/interactive)
  - scheduler next deadline
  - heartbeat fallback
- [x] Add idle gate checks to avoid blocking when immediate work is pending.
- [x] Clamp timeouts for active interaction to maintain smooth feel.

### Unit C: Replace soft spin delay
Files:
- `src/app/GlobalInfo/event_loop.c`

Tasks:
- [x] Replace `SDL_Delay(...)` pacing block with `SDL_WaitEventTimeout(...)`.
- [x] If wait returns an event, process it immediately through normal event path.
- [x] Keep loop behavior deterministic with existing run order.

## Verification
- [x] Build passes (`make`).
- [ ] Idle CPU usage drops compared to spin-loop baseline.
- [ ] Drag/resize/text input remains responsive.
- [ ] No missed input or pane update regressions.

## Completion Notes
- Refactored event handling into:
  - `process_single_input_event(...)`
  - `processInputEvents(ctx, firstEvent)` (supports pre-waited event path)
- Added blocking wait policy:
  - `main_loop_has_immediate_work()`
  - `compute_wait_timeout_ms(...)`
  - `wait_for_next_wake(...)`
- Replaced old soft `SDL_Delay(...)` pacing with `SDL_WaitEventTimeout(...)` in non-rendering iterations.
- Wait timeout now accounts for:
  - active interaction / invalidation frame cadence
  - scheduler next timer deadline
  - heartbeat fallback
- Compile verification passed with `make -j4`.
- Runtime verification remains manual in live IDE session.
