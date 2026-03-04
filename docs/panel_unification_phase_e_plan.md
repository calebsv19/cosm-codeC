# Panel Unification Phase E

Status: complete

Closeout:
- Phase E is complete.
- The planned deep behavioral convergence work described here has been
  implemented.
- Remaining work from this point is optional polish, not unfinished Phase E
  scope.

Purpose:
- Finish the last major behavior/render convergence work now that pane-owned
  state, shared control widgets, shared timing, shared scroll behavior, and the
  shared row-surface foundation are already in place.

Scope:
- This phase is not about more state packaging.
- This phase is about reducing the last bespoke interaction and rendering logic
  in Project, then aligning Tasks and the ControlPanel top shell with the same
  family-level behavior contract.

Out of scope:
- Replacing every panel with one identical renderer.
- Changing panel-specific semantics such as Project's file-vs-directory model.
- Reworking editor panes.

## Current Baseline

Already complete before Phase E:
- Shared pane adapter contract.
- Shared row-surface primitives.
- Shared control widgets and shared text-edit primitives.
- Shared core-time-backed click timing.
- Shared scroll model, including Project-style top-anchor scroll range.
- Pane-owned ControlPanel controller.
- Pane-owned ToolPanel controller with slot-backed state for all seven tool
  subpanels.

Former bespoke areas now covered:
- Project top controls now use the shared control-widget layer.
- Project rename now uses the shared text-edit path.
- Project and Tasks now sit on the shared row activation/render stack.
- ControlPanel now follows the shared top-shell layout contract.

## Success Criteria

Phase E is complete when:
- Project is no longer the main source of bespoke panel interaction plumbing.
- Shared row activation/render contracts can express both flat lists and tree
  rows with extra overlays/states.
- ControlPanel, Git, and Project top controls all use the same layout and hit
  model.
- Shared text-edit/modifier helpers own the normal text-edit path for Project
  rename.
- Tasks is either migrated onto the richer shared tree/list surface or is
  explicitly retained as a thin wrapper over the same shared row/control
  helpers.

## Execution Order

1. Phase E1: Project top controls (complete)
2. Phase E2: Project rename input (complete)
3. Phase E3: Shared row activation contract (complete)
4. Phase E4: Rich row-surface renderer (complete)
5. Phase E5: Project row migration (complete)
6. Phase E6: Tasks convergence decision + implementation (complete)
7. Phase E7: ControlPanel top-shell convergence (complete)

## Phase E1: Project Top Controls

Status: complete

Goal:
- Move Project's top action buttons onto the same shared control-widget layout
  and hit model already used by ControlPanel and Git.

Current gap:
- Project still hand-builds four stacked button rects in
  `src/ide/Panes/ToolPanels/Project/render_tool_project.c`.
- Input still uses local `pointInRect(...)` checks in
  `src/ide/Panes/ToolPanels/Project/input_tool_project.c`.

Deliverables:
- Replace hand-built button layout with shared control layout helpers from:
  - `src/ide/UI/panel_control_widgets.h`
- Replace local button hit checks with shared rect-hit helpers.
- Keep existing commands and semantics unchanged.

Likely files:
- `src/ide/Panes/ToolPanels/Project/render_tool_project.c`
- `src/ide/Panes/ToolPanels/Project/input_tool_project.c`
- `src/ide/Panes/ToolPanels/Project/tool_project.c`
- `src/ide/Panes/ToolPanels/Project/tool_project.h`

Completion checks:
- Project top buttons render through the shared compact control/widget path.
- Button rect ownership lives in slot-backed Project state.
- No Project-local `pointInRect` usage remains for those top controls.

## Phase E2: Project Rename Input

Status: complete

Goal:
- Move Project rename editing onto the same shared text-edit and modifier paths
  already used by ControlPanel search and Git commit input.

Current gap:
- Project rename mode still manually handles:
  - text insertion
  - backspace
  - escape/enter branching
  - modifier detection

Deliverables:
- Replace manual edit-key logic with the shared text-edit helpers from:
  - `src/ide/UI/panel_text_edit.h`
- Replace manual accel detection with shared modifier helpers from:
  - `src/ide/UI/input_modifiers.h`
- Preserve Project-specific rename confirmation/cancel semantics.

Likely files:
- `src/ide/Panes/ToolPanels/Project/input_tool_project.c`
- `src/ide/Panes/ToolPanels/Project/tool_project.c`
- `src/ide/Panes/ToolPanels/Project/tool_project.h`

Completion checks:
- Rename text insertion/deletion/cursor movement is not panel-local anymore.
- Project keeps the same rename UX.
- No raw manual text-edit branch remains except Project-specific command
  dispatch.

## Phase E3: Shared Row Activation Contract

Status: complete

Goal:
- Introduce one shared row-activation shape that both flat-list and tree-style
  panels can use.

Reason:
- Shared row visuals already exist.
- Shared selection helpers already exist.
- The remaining gap is the activation state machine itself.

Shared contract should cover:
- row hit
- prefix hit / expand-toggle hit
- single-click selection
- double-click primary action
- additive/range selection hooks
- optional drag-start hook

Deliverables:
- New shared helper(s) under `src/ide/UI/` for row activation orchestration.
- A minimal callback contract so panel-specific code can decide semantics
  without reimplementing the full click flow.

Likely files:
- new file(s) in `src/ide/UI/`
- `src/ide/Panes/ToolPanels/Project/tool_project.c`
- `src/ide/Panes/ToolPanels/Libraries/tool_libraries.c`
- `src/ide/Panes/ToolPanels/Tasks/tool_tasks.c`

Completion checks:
- Project no longer owns the full row activation state machine by itself.
- At least one additional tree-style panel besides Project uses the same
  activation helper.

## Phase E4: Rich Row-Surface Renderer

Status: complete

Goal:
- Extend the shared row-surface layer so it can express Project's richer row
  states, not just basic selection/hover.

Needed capabilities:
- prefix text region
- primary label text region
- selection fill variants
- hover outline
- drop-target outline
- secondary overlay fills (run target, run ancestor, selected file vs selected
  directory)

Deliverables:
- New shared render helpers layered on top of:
  - `src/ide/UI/row_surface.h`
- A reusable row render spec or config struct suitable for tree rows.

Likely files:
- `src/ide/UI/row_surface.h`
- possibly a new companion implementation/header in `src/ide/UI/`
- `src/ide/Panes/ToolPanels/Project/render_tool_project.c`

Completion checks:
- Project no longer manually assembles most of its row highlight/overlay logic.
- The shared row renderer can support at least one non-Project consumer.

## Phase E5: Project Row Migration

Status: complete

Goal:
- Rebuild Project row behavior on top of the richer shared row-surface and the
  shared row-activation contract, while keeping Project semantics intact.

Preserve:
- folder prefix toggle
- file open on double-click
- file selection
- directory selection
- drag start for files
- directory drop targeting
- run-target highlighting

Deliverables:
- Project row render code becomes primarily data-to-spec conversion.
- Project click handling becomes primarily callback wiring into shared helpers.

Likely files:
- `src/ide/Panes/ToolPanels/Project/render_tool_project.c`
- `src/ide/Panes/ToolPanels/Project/input_tool_project.c`
- `src/ide/Panes/ToolPanels/Project/tool_project.c`

Completion checks:
- Project remains specialized in semantics, not in baseline interaction wiring.
- Most Project row code composes shared helpers instead of hand-owning the
  logic.

## Phase E6: Tasks Convergence

Status: complete

Goal:
- Decide and implement how Tasks fits into the richer shared tree/list surface.

Decision paths:
- Preferred:
  Tasks adopts the richer shared activation/render contract.
- Acceptable fallback:
  Tasks remains bespoke, but only as a thin wrapper over the same shared row
  rendering, row activation, and shared control helpers.

Deliverables:
- Reduce or remove bespoke Tasks click-tree traversal logic where practical.
- Align Tasks button layout with the same shared control helpers used elsewhere.

Likely files:
- `src/ide/Panes/ToolPanels/Tasks/render_tool_tasks.c`
- `src/ide/Panes/ToolPanels/Tasks/input_tool_tasks.c`
- `src/ide/Panes/ToolPanels/Tasks/tool_tasks.c`

Completion checks:
- Tasks no longer feels like a separate UI system.
- Its render and input layers follow the same family-level abstractions as
  Project/ControlPanel/Git.

## Phase E7: ControlPanel Top-Shell Convergence

Status: complete

Goal:
- Finish the last shell-level mismatch so ControlPanel belongs to the same pane
  family as the tool panels, not just the same widget library.

Current gap:
- ControlPanel tree behavior is well aligned.
- Its top search/filter stack still behaves like a special-case header region.

Deliverables:
- Bring ControlPanel top-shell layout onto the same family-level header/control
  contract as the tool panels.
- Reuse the same spacing, strip layout, and section grouping rules where
  possible.

Likely files:
- `src/ide/Panes/ControlPanel/render_control_panel.c`
- `src/ide/Panes/ControlPanel/input_control_panel.c`
- `src/ide/Panes/ControlPanel/control_panel.c`
- `src/ide/UI/panel_control_widgets.h`
- `src/ide/Panes/ToolPanels/tool_panel_top_layout.c`

Completion checks:
- ControlPanel no longer owns a meaningfully separate top-shell layout model.
- Layout edits to the shared panel top shell propagate more naturally across
  ControlPanel and the tool panels.

## Validation Checklist

After each subphase:
- Run `make`.
- Verify hover, selection, double-click, and scroll still work in the affected
  panels.
- Verify no clipped text regression inside scrollable regions.
- Verify the last row in long scroll surfaces can still reach the top.

Before marking Phase E complete:
- Test Project:
  - folder expand/collapse
  - file open on double-click
  - rename
  - drag to editor
  - drag to directory
  - run-target highlight behavior
- Test Tasks:
  - selection
  - rename
  - scroll
  - add/remove
- Test ControlPanel:
  - search
  - filters
  - symbol double-click navigation

## Risk Notes

Highest-risk area:
- Project row activation migration, because drag/drop, selection, and
  expand/collapse all overlap.

Main rule:
- Preserve semantics first, then reduce bespoke code.
- If a shared abstraction starts forcing Project-specific behavior into awkward
  branches, widen the abstraction instead of hiding logic in special cases.
