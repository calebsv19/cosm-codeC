# Phase C.5 Completion: Match Markers + Projection Scope

## Goal
Add visual match previews and ensure projection updates apply across open files/views by default, not just the active file.

## Shipped
- Added match marker rendering in editor:
  - scrollbar ticks when content overflows viewport
  - inline right-edge markers when file fits viewport
  - marker source: `projection.realMatchLines`
  - file: `src/ide/Panes/Editor/Render/render_editor.c`
- Added projection sync scope behavior in editor view model:
  - default scope: all open files across all open editor views
  - optional manual code toggle for active-only scope
  - APIs:
    - `editor_projection_set_scope_all_open_files(bool)`
    - `editor_projection_scope_all_open_files(void)`
  - file: `src/ide/Panes/Editor/editor_view.c`
  - declarations: `src/ide/Panes/Editor/editor_view.h`
- Updated projection sync pass:
  - traverses editor split tree and applies projection mode/rebuild to all leaf open files when default scope is enabled
  - still supports active-only behavior when toggle is disabled

## Done Criteria Check
- Scrollbar markers appear for projected matches when file is scrollable: yes.
- Inline markers appear when file fits without scrollbar: yes.
- Projection updates apply across all open files/views by default: yes.
- Code-level scope toggle for future UI/keybind wiring exists: yes.
- Build passes: yes.

## Deferred To Next Step
- UI-level toggle for active-only vs all-open projection scope.
- Performance guardrails/debounce/fan-out optimization for very large open-file sets.
