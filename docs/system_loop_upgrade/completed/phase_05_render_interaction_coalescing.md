# Phase 05 Plan: Render/Interaction Coalescing

## Goal
Reduce wake/render thrash during high-frequency input and background bursts while preserving smooth interaction.

## Scope
In scope:
- Coalesce mouse motion bursts to latest-position processing.
- Improve active-interaction detection for timeout policy.
- Add bounded worker-message drain per iteration to avoid burst stalls.
- Keep dirty/invalidation contract intact.

Out of scope:
- Full instrumentation/overlay reporting (Phase 06).
- Deep renderer algorithm changes.

## Implementation Checklist

### Unit A: Mouse motion coalescing
Files:
- `src/app/GlobalInfo/event_loop.c`

Tasks:
- [x] Coalesce consecutive `SDL_MOUSEMOTION` events in the same drain pass.
- [x] Process only latest motion before the next non-motion event/end-of-drain.
- [x] Preserve invalidation and hover behavior.

### Unit B: Active interaction pacing refinements
Files:
- `src/app/GlobalInfo/event_loop.c`

Tasks:
- [x] Treat pane-resize dragging as active interaction.
- [x] Keep short wait clamp under active interaction.
- [x] Preserve low-idle blocking behavior when interaction is inactive.

### Unit C: Worker message burst handling
Files:
- `src/app/GlobalInfo/event_loop.c`

Tasks:
- [x] Add drain budget per loop iteration (avoid long stalls in one frame).
- [x] If queue remains non-empty after budget, leave work for next iteration.
- [x] Keep immediate-work guard so loop does not deep-sleep while messages remain.

## Verification
- [x] Build passes (`make`).
- [ ] Mouse-move heavy interactions remain smooth.
- [ ] No regressions in hover/selection behavior.
- [ ] Idle CPU remains low after startup settles.

## Completion Notes
- Added mouse motion burst coalescing in `processInputEvents(...)`:
  - retain latest `SDL_MOUSEMOTION`
  - process once before next non-motion event/end-of-drain
- Added active interaction signal for pane resize dragging (`gResizeDrag.active`) in timeout policy.
- Added worker message drain budget (`kDrainBudget = 64`) to avoid long single-iteration stalls.
- Existing immediate-work checks keep queue backlog from entering deep wait.
- Compile verification passed with `make -j4`.
- Remaining checks are runtime/manual in live IDE.
