# Phase C.4 Completion: Projection Input Mapping + Read-Only

## Goal
Enable projection-aware navigation and protect files from accidental edits while projection mode is active.

## Shipped
- Added projection row mapping helper:
  - `editor_projection_map_row_to_source(...)` in `src/ide/Panes/Editor/editor_view.c`
  - declaration in `src/ide/Panes/Editor/editor_view.h`
- Projection activation gate now uses search query text only:
  - `editor_sync_active_file_projection_mode(...)` in `src/ide/Panes/Editor/editor_view.c`
  - projection turns on only when `control_panel_get_search_query()` is non-empty
- Mouse input mapping in projection mode:
  - click position resolves against projected rows
  - clicked projection row maps to real source cursor target
  - file selection dragging is disabled in projection mode
  - changes in `src/ide/Panes/Editor/Input/editor_input_mouse.c`
- Projection-aware scroll math in input:
  - wheel and scrollbar drag now use active render-source line count
  - changes in `src/ide/Panes/Editor/Input/editor_input_mouse.c`
- Read-only enforcement for projection mode:
  - blocks mutate commands: newline/delete/delete-forward/tab/cut/paste/undo/redo/text input
  - Enter in projection mode jumps to source and exits projection view for that file
  - changes in `src/ide/Panes/Editor/Commands/command_editor.c`
- Additional keyboard guard:
  - blocks projection-mode alt/cmd mutation-style paths in keydown path
  - blocks direct text input in projection mode
  - changes in `src/ide/Panes/Editor/Input/editor_input_keyboard.c`

## Done Criteria Check
- Empty search query disables projection mode: yes.
- Clicking projected rows maps to source cursor location: yes.
- Enter in projection mode jumps to source context: yes.
- Projection mode is read-only for text mutation commands: yes.
- Build passes: yes.

## Deferred To Next Step
- Match marker rendering on scrollbar/in-pane.
- Optional richer projected-row keyboard navigation model.
