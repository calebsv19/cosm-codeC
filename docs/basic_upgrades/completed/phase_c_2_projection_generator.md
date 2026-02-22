# Phase C.2 Completion: Projection Generator + Mapping

## Goal
Implement the projection generator so active editor files can build temporary search projections with row-to-source mapping and real match line tracking.

## Shipped
- Added control-panel filter options export API:
  - `control_panel_get_search_filter_options(...)` in `src/ide/Panes/ControlPanel/control_panel.c`
  - declaration in `src/ide/Panes/ControlPanel/control_panel.h`
- Added projection generator module:
  - `src/ide/Panes/Editor/editor_projection.h`
  - `src/ide/Panes/Editor/editor_projection.c`
- Generator behavior:
  - reuses current symbol filter mode + field semantics
  - builds projected rows from symbol matches (`[kind] name (line N)` + context rows)
  - adds line-text fallback matches for `SYMBOLS` mode query
  - stores:
    - `projection.lines`
    - `projection.projectedToRealLine`
    - `projection.projectedToRealCol`
    - `projection.realMatchLines`
    - `projection.realMatchCount`
    - `projection.buildStamp`
  - emits a stable empty state row: `No matches for "..."` when no results
- Wired active-file rebuild path:
  - `editor_sync_active_file_projection_mode(...)` in `src/ide/Panes/Editor/editor_view.c`
  - when search is inactive: forces real mode and clears projection
  - when search is active: sets projection mode and rebuilds projection from current query/options

## Done Criteria Check
- Active file receives projection data when search is active: yes.
- Projection rows include source mapping arrays: yes.
- Real match line list is populated for future marker rendering: yes.
- Search off disables projection and clears state: yes.

## Deferred To Next Step
- Render-path switch to draw projected rows in editor pane.
- Projection-mode navigation mapping for mouse/keyboard.
- Visual marker rendering (scrollbar/in-pane).
