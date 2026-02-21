# Phase 6.2 Plan: Event-Driven Render Gate

## Goal
Render only when invalidated (or when periodic fallback cadence is required), replacing unconditional redraw behavior.

## Scope
In scope:
- Convert `checkRenderFrame(...)` to invalidation-first logic.
- Add idle sleep/yield strategy for non-invalidated frames.
- Add bounded fallback redraw cadence (safety heartbeat).

Out of scope:
- Pane texture caching.
- Deep refactor of renderer backend internals.

## Implementation Checklist

### Unit A: Render gate policy
Files:
- `src/app/GlobalInfo/event_loop.c`

Tasks:
- [x] Update render gate to check invalidation state before rendering.
- [x] Keep frame cap policy for invalidated frames.
- [x] Add heartbeat redraw (example: every 250-500ms) for safety/animations.

### Unit B: Invalidation consumption
Files:
- `src/app/GlobalInfo/event_loop.c`
- `src/app/GlobalInfo/core_state.*`

Tasks:
- [x] Consume/clear invalidation flags only after successful render.
- [x] Preserve full-redraw flags until processed.
- [x] Increment frame counters for observability.

### Unit C: Overlay and transient behavior
Files:
- `src/engine/Render/render_pipeline.c`
- popup/drag state modules

Tasks:
- [x] Ensure rename popup/drag overlays trigger invalidation while active.
- [x] Ensure cursor blink/timers trigger minimal required invalidation.

## Verification
- [x] Idle CPU usage drops in perf run.
- [x] TimerHUD `RenderGate`/`Render` drops significantly when idle.
- [x] No missed redraws on hover/type/scroll/resize/popup.

## Completion Gate
- Event-driven gating is default.
- Visual correctness preserved.
- Ready for pane-level cache/selective redraw in Phase 6.3.

## Current Status
- Core 6.2 gate conversion is implemented in `src/app/GlobalInfo/event_loop.c`.
- Rename caret/error animation now invalidates only on visual state changes (no per-frame forced rename invalidation).
- Input invalidation now filters for visual-impacting SDL events instead of invalidating on every polled event.
- Completed.
