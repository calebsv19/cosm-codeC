# North Star: Invalidation-Driven Rendering

## Goal
Move from "render everything each frame" to "render only when needed", with pane-level invalidation first and full-frame fallback when required.

## Why This Is the New Main Plan
Current TimerHUD runs are still dominated by:
- `Render` / `PaneRender` (~50ms class total frame cost in recent runs)

Current code state confirms full redraw behavior:
- `runFrameLoop` always reaches render gate by frame interval (`src/app/GlobalInfo/event_loop.c`)
- `RenderPipeline_renderAll` loops through every pane every rendered frame (`src/engine/Render/render_pipeline.c`)
- `UIPane` has no invalidation/dirty metadata (`src/ide/Panes/PaneInfo/pane.h`)
- `IDECoreState` has no global render invalidation tracker (`src/app/GlobalInfo/core_state.h`)

## Success Criteria
- Idle loop does not redraw panes unless invalidated.
- Input-triggered updates redraw only affected panes when feasible.
- Full redraw is reserved for layout/window/theme/global changes.
- TimerHUD shows major reduction in steady-state `Render` / `PaneRender`.
- Behavior parity: editor, terminal, tool panels, popups, drag overlays remain correct.

## Execution Model
1. Introduce invalidation API and dirty reasons (global + per-pane).
2. Gate rendering based on invalidation instead of fixed unconditional redraw cadence.
3. Add pane render caching path (render-to-texture or equivalent) for unchanged panes.
4. Route input/background/layout events to precise invalidation signals.
5. Validate with TimerHUD + functional checklists and promote defaults.

## Sub-Phase Roadmap
1. Phase 6.1: Invalidation contract and state plumbing.
2. Phase 6.2: Render-gate conversion (event/dirty driven).
3. Phase 6.3: Pane cache path + selective redraw.
4. Phase 6.4: Hardening, tuning, and regression guardrails.

## Active Phase
- `docs/render_speed_upgrade/phases_phase6/phase_06_04_hardening_and_guardrails.md` (completed)

## Current Status
- Phase 6.1-6.4 shipped and validated as the current default path.
- 6.3 Unit B (true cache reuse/rebuild split) remains deferred due to Vulkan structural constraints.
- Current operating model: invalidation-driven render gate + targeted pane invalidation + hardening guardrails.

## Working Rules
- Keep full-redraw fallback available behind a runtime toggle until Phase 6.4.
- Every new invalidation trigger must name a reason (input/layout/theme/content/overlay).
- Each phase must ship with TimerHUD before/after notes and behavior checks.
