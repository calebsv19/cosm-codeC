# Panel Unification Plan

This document tracks the phased unification of the ControlPanel and ToolPanel
rendering/input foundations. The first pass focused on standardizing base pane
ownership, timing, and visual primitives. The next pass is deeper convergence:
shared panel contracts, shared row surfaces, shared state ownership, and the
remaining Project/Tasks outliers.

Final state:
- The foundation phases and the follow-on execution phases are now complete.
- This document remains as the historical overview of the unification effort.
- Remaining work from this point is optional visual/runtime polish, not
  structural panel-system unification.

## Current Status

- Foundation Phase 1 is complete.
- Foundation Phase 2 is complete.
- Foundation Phase 3 is complete.
- Foundation Phase 4 is complete.
- The original Phase 5 was split into follow-on execution phases, and those
  execution phases are now complete.
- The implementation record lives in `docs/panel_unification_execution_plan.md`.

Completed so far:
- Base pane fill ownership is centralized in the outer pane render wrappers.
- ControlPanel and ToolPanel content renderers no longer repaint the shared pane
  fill.
- Shared double-click timing now routes through `core/LoopTime`, which in turn
  uses the shared `core_time` runtime.
- Shared tree surfaces now use Project's current `14px` row spacing.
- Libraries and Tasks have been aligned to the same `14px` tree row spacing.
- ControlPanel now uses the shared header/body split fill helper used by newer
  tool panels.
- Assets, BuildOutput, and Errors were moved to a denser visual baseline to
  better match the tree-family panels.
- Shared visual metrics now live in `ide/UI/panel_metrics.h` so row-height and
  tree-indent values do not drift per panel.
- BuildOutput now uses shared scroll state, wheel scrolling, thumb dragging,
  and a rendered scrollbar instead of being the scrollless outlier.
- Shared scroll input consumption now lives in `ide/UI/scroll_input_adapter.h`
  and is used by ControlPanel, Project, Assets, Libraries, Errors, Git, and
  BuildOutput.
- Shared bounded flat-list selection now lives in
  `ide/UI/flat_list_selection.h` and is used by BuildOutput, Errors, and
  Libraries.
- Shared text-field focus state transitions now live in
  `ide/UI/text_input_focus.h` and are used by ControlPanel search and the Git
  commit message field.
- Shared flat-list row hit-testing now lives in `ide/UI/flat_list_hit_test.h`
  and is used by Assets, Errors, and BuildOutput.
- Shared modifier checks now live in `ide/UI/input_modifiers.h` and are used
  by ControlPanel, Git, Assets, Errors, and BuildOutput.
- Shared flat-list drag-selection state now lives in
  `ide/UI/flat_list_interaction.h` and is used by Assets, Errors, and
  BuildOutput.
- Shared editor navigation now lives in `ide/UI/editor_navigation.h` and is
  used by BuildOutput, Errors, Libraries, Assets, and Project for
  open-and-jump/open-in-editor flows.

Known remaining gaps after the foundation pass:
- There is still no shared panel descriptor/adapter contract; ToolPanel still
  dispatches through hardcoded switch routing.
- There is still no shared row-surface renderer/activator for bespoke tree and
  list panels.
- ControlPanel still owns a custom top-area layout for search and filter
  controls instead of using a shared control widget layer.
- ControlPanel still owns a custom editor-target resolution path instead of
  fully reusing the newer shared editor navigation helper.
- Project still uses a bespoke tree/input model with mixed file/folder
  selection, drag and drop, rename flow, and special directory targeting.
- Tasks still uses a custom renderer/input path and remains the least unified
  panel.
- State ownership is still fragmented across module globals/statics instead of
  pane-instance-owned controller state.

Those gap notes are historical. They were accurate at the end of the
foundation-only pass, but they have since been addressed by the completed
execution phases.

## Phase 1: Render Ownership

- Define one owner for base pane fill/chrome.
- Remove duplicate pane fill calls inside ControlPanel/ToolPanel content renderers.
- Keep panel-specific content renderers focused on content only.

## Phase 2: Shared Interaction Timing

- Replace panel-local `SDL_GetTicks()` double-click checks with a shared helper.
- Route the helper through `core/LoopTime`, which already wraps the shared
  `core_time` runtime used by the engine.
- Use one default double-click threshold across panel interactions unless a
  panel has a documented reason to diverge.

## Phase 3: Shared Visual Baseline

- Align header/body split handling.
- Align padding, row spacing, text tiers, and collapse marker spacing.
- Normalize shared input/button baseline sizing.

## Phase 4: Shared Interaction Shape

- Normalize hover ownership, selection activation, scroll handling, and text
  input focus patterns.
- Reduce per-panel duplication around click, double-click, and row activation.

## Follow-On Execution Phases

The old "Phase 5" was too broad. It is now broken into explicit execution
phases in `docs/panel_unification_execution_plan.md`:

- Execution Phase A: Shared panel contract/adapter layer.
- Execution Phase B: Shared row-surface render and activation layer.
- Execution Phase C: Shared control widgets and text-input routing.
- Execution Phase D: Shared pane state ownership and data flow.
- Execution Phase E: Deep behavioral convergence across Project, Tasks, and the
  remaining panel-specific interaction layers.

All follow-on execution phases listed above are now complete.
