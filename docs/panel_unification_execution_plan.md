# Panel Unification Execution Plan

This document is the active implementation checklist for the deeper convergence
work after the foundation pass. It turns the old single "Phase 5" into a set of
smaller phases that can be completed and verified incrementally.

Update:
- All planned execution phases in this document are now complete.
- This document remains as the implementation record for the unification work.
- Any follow-up work from this point should be treated as targeted polish or
  future feature work, not unfinished panel-system convergence.

## Goal

Make ControlPanel and ToolPanels behave like one coherent panel system:

- one shared outer panel contract
- one shared row-surface layer for tree/list content
- one shared control-widget layer for top-area controls
- one consistent pane-owned state model
- one standardized input and activation path

The intent is that visual and structural changes to one panel family should
propagate through shared layers instead of requiring panel-by-panel rewrites.

## Current Baseline

Already complete:

- Base pane fill ownership is unified.
- Shared interaction timing uses `core/LoopTime` and the shared `core_time`
  backend for double-click behavior.
- Shared visual metrics exist for dense row spacing and tree indentation.
- Shared scroll consumption is in place across the major panel families.
- Shared flat-list selection, hit-testing, drag-selection, modifier checks, and
  editor navigation helpers exist.
- ControlPanel tree rendering and Git tree rendering already share the tree
  surface path.
- All seven ToolPanel subpanels now use slot-backed pane-owned controller
  state.
- All seven ToolPanel subpanels now use the shared scroll model.
- Assets, Errors, BuildOutput, Libraries, Tasks, Project, and Git all now use
  the shared control/row/activation stack to the degree appropriate for their
  panel semantics.
- ControlPanel and Git tree clicks now use the shared tree hit-test plus
  shared row-activation pattern, and the legacy tree click wrappers have been
  removed from the tree layer.

Residual follow-up ideas:

- Optional visual polish passes can still refine spacing, hit-target tuning,
  and typography if new regressions are noticed during runtime use.
- Future panel additions should continue to use the shared adapter, controller
  slot, control-widget, row-surface, row-activation, and shared tree helpers.

## Status

- Execution Phase A: complete
- Execution Phase B: complete
- Execution Phase C: complete
- Execution Phase D: complete
- Execution Phase E: complete
- Overall panel unification program: complete

## Execution Phase A: Shared Panel Contract

Status: complete

Purpose:
- Replace switch-heavy panel dispatch with a shared panel descriptor/controller
  contract so ControlPanel and ToolPanels can plug into the same outer shell.

Deliverables:
- Define a shared panel adapter/descriptor type for render, mouse, hover,
  scroll, keyboard, text-input, and optional command hooks.
- Move ToolPanel subpanel dispatch behind adapter lookup instead of hardcoded
  switch routing.
- Define an equivalent adapter path for ControlPanel so it uses the same outer
  interaction contract.

Completion criteria:
- ToolPanel no longer needs direct switch routing for each event type.
- New panels can be attached by registering an adapter instead of editing
  multiple switch statements.
- ControlPanel and ToolPanel are both described through the same outer contract.

Progress notes:
- Complete: shared `UIPanelViewAdapter` contract added in
  `src/ide/Panes/panel_view_adapter.h`.
- Complete: ToolPanel render/input/command routing now resolves the active view
  through one adapter lookup instead of separate switch dispatch per event type.
- Complete: ControlPanel now has a matching adapter path for render, input, and
  command routing via `control_panel_adapter`.

Suggested order:
1. Introduce the adapter type and registration table.
2. Convert ToolPanel routing first.
3. Move ControlPanel onto the same outer adapter path.

## Execution Phase B: Shared Row Surface

Status: complete

Purpose:
- Build one reusable row-surface layer that can render and activate both tree
  and flat-list content without forcing every panel into the exact same model.

Deliverables:
- Shared row render helpers for selection fill, hover outline, text baseline,
  prefix/collapse marker spacing, clipping, and activation regions.
- Shared activation helpers for click, double-click, expand/collapse, and row
  primary action callbacks.
- A row descriptor format that supports:
  - plain rows
  - tree rows
  - rows with prefix toggles
  - rows with secondary metadata or multi-line blocks

Migration targets:
- First: Tasks
- Then: Libraries / Assets / Errors / BuildOutput alignment where useful
- Last: Project

Completion criteria:
- Tasks no longer uses a one-off tree renderer/input path.
- Shared row visuals are the default for bespoke list/tree panels.
- Project can reuse at least part of the same row-surface primitives even if
  its data model remains specialized.

Progress notes:
- Complete: shared row-surface primitives now live in
  `src/ide/UI/row_surface.h` for row bounds, clipping, hover outlines,
  selection visuals, and hit regions.
- Complete: Tasks now builds one shared row layout for render and input
  hit-testing instead of duplicating row geometry in separate paths.
- Complete: Tasks hover input now routes directly through shared row-hit logic
  instead of fabricating a fake mouse-motion event.
- Complete: Libraries, Assets, Errors, and BuildOutput now render row selection
  and row highlight geometry through the shared row-surface layer.
- Complete: Project now reuses the shared row-surface layer for row clipping and
  hover/select-all visuals while preserving its specialized file/folder logic.

## Execution Phase C: Shared Control Widgets

Status: complete

Purpose:
- Unify the top-area UI primitives so search fields, message fields, button
  rows, collapsible section headers, and filter groups are all built from shared
  widgets instead of custom panel-local drawing.

Deliverables:
- Shared panel text-field widget for focus, caret, placeholder, text insert,
  delete, cursor movement, and clipping.
- Shared button-row/control-strip helpers for small top-area action controls.
- Shared collapsible section header helper.
- Shared filter chip/button group widget if ControlPanel keeps multi-row filter
  groups.

Migration targets:
- First: Git commit message field and ControlPanel search field.
- Then: ControlPanel filter header and filter button groups.
- Then: Project and Tasks top action buttons where beneficial.

Completion criteria:
- ControlPanel search and Git message field use the same text-field widget.
- ControlPanel top-area layout is no longer a one-off renderer for common
  controls.
- Top-area control hit-testing is driven by shared widget state instead of
  scattered custom rectangles.

Progress notes:
- Complete: shared panel text-field widget now lives in
  `src/ide/UI/panel_text_field.h`.
- Complete: ControlPanel search and the Git commit field now share the same
  text-field render/caret path while keeping their existing input state.
- Complete: shared compact control widgets now live in
  `src/ide/UI/panel_control_widgets.h`.
- Complete: ControlPanel search action buttons, collapsible filter header,
  and filter chip rows now render through shared control widget helpers instead
  of bespoke drawing code.
- Complete: ControlPanel top-strip layout now uses shared widget layout
  helpers for the search field + action buttons instead of storing separate
  hand-computed rectangles.
- Complete: ControlPanel search field, search action buttons, and the
  collapsible filter header now use shared rect-hit helpers instead of panel-
  local point-in-rect functions.
- Complete: ControlPanel filter chip rows now register and hit-test through
  shared tagged-rect list helpers, so the panel logic no longer owns the raw
  filter-chip hit box bookkeeping.
- Complete: shared text-buffer edit primitives now live in
  `src/ide/UI/panel_text_edit.h`.
- Complete: ControlPanel search and the Git commit field now share the same
  text insert/delete/cursor movement logic and SDL text/edit-key routing
  helpers, while still preserving their panel-specific side effects.
- Complete: Git's add-all / message / commit strip now uses the same shared
  strip-layout and rect-hit helpers as ControlPanel, and its top action buttons
  render through the shared compact control widget path.

## Execution Phase D: Shared State Ownership And Data Flow

Status: complete

Purpose:
- Reduce reliance on module-global state so pane behavior and rendering are
  driven by pane-owned controller state and shared view models.

Deliverables:
- Define pane-owned controller/state structs for ControlPanel and each tool
  panel family.
- Reduce `extern` and module-static state dependencies in render/input paths.
- Separate view model building from rendering where the panel currently mutates
  or refreshes model state during render.
- Centralize editor navigation and panel activation targets through shared
  helpers instead of panel-local view resolution.

Migration targets:
- First: ControlPanel state packaging.
- Then: Git/Libraries state packaging.
- Then: Project state packaging.

Completion criteria:
- New panel instances could be added without hidden module-global conflicts.
- Renderers consume prepared state instead of rebuilding core state ad hoc.
- ControlPanel no longer owns a custom editor-target resolution branch.

Progress notes:
The entries below record the implementation sequence and retain the wording
used during execution.
- In progress: ControlPanel's previously scattered file-scope globals are now
  grouped into explicit controller-state buckets in
  `src/ide/Panes/ControlPanel/control_panel.c`:
  tree state, UI state, filter/config state, and cache state.
- In progress: the existing ControlPanel API still routes through the same
  functions, but it now reads/writes grouped state instead of independently
  declared globals, which reduces the hidden state surface for later migration.
- In progress: Git's local controller state is now grouped into explicit UI and
  watcher-state buckets in `src/ide/Panes/ToolPanels/Git/tool_git.c` instead of
  separate file-scope statics.
- In progress: ControlPanel symbol-tree refresh is now triggered in
  `control_panel_adapter` before render dispatch, so
  `renderControlPanelContents()` is display-only and no longer mutates the tree
  model directly during draw.
- In progress: shared editor navigation now owns the "find existing tab or pick
  the best editor leaf" logic in `src/ide/UI/editor_navigation.h`, and
  ControlPanel double-click navigation now uses that shared path instead of a
  panel-local editor view search.
- In progress: ControlPanel pre-render model prep is now encapsulated behind
  `control_panel_prepare_for_render(...)`, so the adapter no longer needs to
  know the panel's project-root / active-file refresh details directly.
- In progress: Git's remaining shared model data is now hidden behind accessors
  (`git_panel_branch_name`, `git_panel_file_at`, `git_panel_log_at`, etc.)
  instead of exposing raw model arrays/counters to other files.
- In progress: ControlPanel now captures and restores the logical tree
  selection after a real symbol-tree rebuild, and re-applies the select-all
  visual state when possible, which reduces visible tree reset/glitch behavior
  during analysis-driven refreshes.
- In progress: ControlPanel now keeps base analysis-tree rebuilds and visible
  filtered-tree rebuilds on separate paths: analysis refresh marks the visible
  tree dirty with a pending restore snapshot, while the filtered tree rebuilds
  through its own sync path.
- In progress: filtered symbol-tree clones now re-apply cached expansion state
  from the shared expansion cache, so visible-tree rebuilds preserve user
  collapse/expand choices more consistently instead of falling back to stale
  base-tree expansion flags.
- In progress: ControlPanel now short-circuits logically identical base-tree
  rebuilds and logically equivalent filtered-tree rebuilds, which reduces
  unnecessary tree replacement, row churn, and visible row-position jumping
  during large analysis refreshes.
- In progress: Git model sync now runs through `git_panel_prepare_for_render()`
  in the tool-panel adapter instead of mutating state inside
  `renderGitPanel()`, so the Git renderer is display-only.
- In progress: Git tree/scroll ownership now lives in `tool_git.c` behind
  accessors (`git_panel_tree`, `git_panel_scroll`, etc.) instead of render-file
  globals shared through `extern`.
- In progress: `UIPane` now has a generic controller-state slot plus a destroy
  hook, so pane instances can own controller objects directly instead of only
  relying on hidden module statics.
- In progress: the persistent ToolPanel pane now attaches one pane-owned
  `ToolPanelControllerState`, and tool subpanels allocate their own state
  inside named slots of that controller instead of competing for the pane's
  single controller pointer.
- In progress: `tool_git.c` now stores its grouped model/UI/tree/watcher state
  inside the ToolPanel controller's Git slot instead of static file-scope
  storage.
- In progress: Libraries state now lives inside the ToolPanel controller's
  Libraries slot instead of a private module-global singleton, while preserving
  the existing `libraries_panel_state()` API for callers.
- In progress: Assets now resolves its catalog, selection state, scroll state,
  control-strip rects, and drag/double-click trackers through a ToolPanel-owned
  Assets slot instead of separate module statics across `tool_assets`,
  `render_tool_assets`, and `input_tool_assets`.
- In progress: BuildOutput now resolves its list-interaction state (scroll,
  selection, drag, double-click) through a ToolPanel-owned UI slot, and its
  persistent build-output tree/selection state through a ToolPanel-owned panel
  slot instead of separate static singletons.
- In progress: Errors now resolves its snapshot cache, filters, selection,
  scroll state, control-button rects, collapse state, and drag/double-click
  state through a ToolPanel-owned Errors slot instead of file-scope statics.
- In progress: the persistent ControlPanel pane now attaches a pane-owned
  ControlPanel controller object during pane initialization, and
  `control_panel.c` now resolves its grouped tree/UI/filter/cache state through
  that pane-owned controller instead of static file-scope storage.
- In progress: Libraries panel state is no longer exported as a global; callers
  now access it through `libraries_panel_state()`, which narrows the shared
  state surface and removes another cross-file global dependency.
- In progress: ControlPanel's visible-tree dirty flag and pending restore
  snapshot now live inside the explicit tree-state bucket instead of separate
  file-level coordination flags, which makes the tree sync path a single
  coherent state unit.
- In progress: Tasks now resolves its task roots, selection/hover state, and
  inline rename/edit buffer through a ToolPanel-owned Tasks slot instead of
  file-scope statics, so its tree model matches the shared ToolPanel
  controller ownership model.
- In progress: Project now resolves its hover/selection pointers, button
  hit-rects, rename buffers, run-target buffer, scroll state/rects, and cached
  expansion metadata through a ToolPanel-owned Project slot instead of direct
  globals spread across render/input/tool files.
- In progress: Project's renderer no longer owns static scrollbar state, and
  the drag-start timestamp now uses `loop_time_now_ms32()` so the remaining
  Project interaction timing follows the shared core loop-time clock instead of
  `SDL_GetTicks()`.
- Complete: ToolPanel dispatch now binds the actual `UIPane*` through
  render/input/command entry points before delegating to subpanel adapters, so
  slot-backed tool-panel state resolves from the explicit pane being handled in
  normal flow instead of reaching back through `getUIState()`.
- Complete: `tool_panel_adapter` now exposes explicit pane-based slot access
  (`tool_panel_ensure_state_slot_for_pane(...)`) and keeps the old implicit
  slot lookup only as a compatibility fallback for non-dispatch callers.
- Complete: Git, Libraries, Assets, BuildOutput (UI + panel slots), Errors,
  Tasks, and Project now all resolve slot-backed state through the same shared
  `tool_panel_resolve_state_slot(...)` helper, so every ToolPanel subpanel uses
  the same slot lookup and bootstrap fallback path instead of hand-rolled
  per-panel state resolution.

## Execution Phase E: Deep Behavioral Convergence

Status: complete

Purpose:
- Finish the remaining behavior/render convergence work now that the shared
  contract, control widgets, row primitives, and pane-owned state layers are in
  place.

Why last:
- The remaining gaps are no longer about state ownership. They are about
  unifying the final panel-specific behavior contracts, with Project still the
  largest specialized pane and ControlPanel/Tasks as the follow-through.

Deliverables:
- Move Project top controls onto the shared control-widget layout and hit model.
- Move Project rename/edit input onto the shared text-edit and modifier
  helpers.
- Introduce a richer shared row-activation contract for tree/list panels.
- Introduce a richer shared row-surface renderer that supports overlays/states
  needed by Project.
- Migrate Project onto that richer shared row/activation layer while preserving
  file/folder/drop-target semantics.
- Decide whether Tasks should adopt that richer shared tree/list surface or
  remain a thinner bespoke consumer of the shared row/control helpers.
- Finish ControlPanel top-shell convergence so its header/control area matches
  the same family-level layout contract as the tool panels.

Completion criteria:
- Project remains behaviorally specialized but no longer owns the main bespoke
  panel interaction path.
- Shared row activation/render helpers are the default for both flat-list and
  tree-style panels.
- ControlPanel, Git, and Project top controls share the same layout/hit model.
- Panel-specific code primarily selects semantics while shared layers own
  routing, rendering, and baseline interaction behavior.

Progress notes:
- Complete: Project top controls now render through the shared compact control
  widget path and use the shared vertical button-stack layout plus shared
  tagged rect-hit helper instead of hand-built button rects and local
  `pointInRect` checks.
- Complete: Project inline rename editing now uses the shared text-edit buffer
  helpers for text input, deletion, and cursor movement, and its keyboard
  accel detection now uses the shared input modifier helper instead of a local
  modifier branch.
- Complete: shared row activation now lives in `src/ide/UI/row_activation.h`,
  and both Project and Libraries route primary row click handling through that
  shared contract for selection, prefix toggles, double-click activation, and
  optional drag-start behavior.
- Complete: the shared row-surface layer now exposes a spec-driven render path
  for layered fills and outlines, and Project plus Libraries both use that
  richer renderer instead of manually stacking most row highlight/overlay
  drawing inline.
- Complete: Project row render is now driven by a dedicated row visual-state
  builder that converts entry state into shared row-surface specs, and its
  click path now reduces to prefix-hit resolution plus shared row-activation
  callback wiring.
- Complete: Tasks now uses the shared compact control widgets for its top
  controls, the shared row-surface renderer for row highlight paint, and the
  shared row-activation contract for label/expand hits, while preserving task
  checkbox toggles as a task-specific secondary row action.
- Complete: ControlPanel's top shell now uses the shared ToolPanel layout
  defaults for padding, control-row anchoring, header spacing, split-content
  baseline, and body darkening, so its header/control area follows the same
  family-level shell contract as the tool panels.

Execution detail:
- Use `docs/panel_unification_phase_e_plan.md` as the active implementation
  checklist for Phase E subphases, file targets, and validation passes.

## Tracking Notes

Use this file as the active checklist during implementation:

- Mark a phase complete only when its completion criteria are satisfied.
- If a phase needs to be split further, add sub-phases rather than folding new
  work into an existing phase without documenting it.
- Keep the foundation tracker in `docs/panel_unification_plan.md` as the
  historical record of what was already completed.
