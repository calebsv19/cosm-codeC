# Phase C.1 Completion: Projection State + Lifecycle

## Goal
Ship Phase C step 1 by adding projection data/state to editor files and wiring lifecycle-safe toggling for the active file based on search state.

## Shipped
- Added projection model types in `src/ide/Panes/Editor/editor_view.h`:
  - `EditorRenderSource` (`EDITOR_RENDER_REAL`, `EDITOR_RENDER_PROJECTION`)
  - `SearchProjection` (projected lines, mapping arrays, match lines, `buildStamp`)
- Extended `OpenFile` with:
  - `bufferVersion`
  - `renderSource`
  - `projection`
- Added lifecycle helpers in `src/ide/Panes/Editor/editor_view.c`:
  - `editor_projection_reset(...)`
  - `editor_projection_free(...)`
  - `editor_invalidate_file_projection(...)`
  - `editor_set_file_render_source(...)`
  - `editor_file_projection_active(...)`
  - `editor_sync_active_file_projection_mode(...)`
- Integrated cleanup/invalidation:
  - projection freed when open file is released
  - projection invalidated on file modified / file reload
  - `bufferVersion` incremented on modification/reload paths
- Added control-panel search state API:
  - `control_panel_has_active_search_state()` in `src/ide/Panes/ControlPanel/control_panel.c`
- Wired active-file projection mode sync on search/filter changes:
  - insert/backspace/delete/clear search query
  - filter button activation
  - control panel reset

## Done Criteria Check
- Projection state exists per open file: yes.
- Projection lifecycle helpers exist and are called in create/update/free paths: yes.
- Active file render source toggles based on search state: yes.
- No render/input projection behavior added yet (reserved for C.2+): yes.

## Deferred To Next Step
- Projection content generator (actual projected lines/mappings population).
- Render-path switch to draw projected lines.
- Projection navigation mapping and read-only behavior.
