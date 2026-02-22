# Phase C Plan: Editor Search Projection + Match Preview

## Goal
Turn search into a fast read-navigation mode for open files:
- Keep the real file buffer unchanged.
- Build a temporary projected view that reorders/shows only matching lines or symbol blocks.
- Add match indicators on the editor scrollbar (or direct line markers when file fits) so users can quickly scan where matches live.

## Why this phase replaces old C
- Symbol tree filtering is already usable enough for now.
- The next highest-value workflow is in-file discovery: finding methods, vars, or tagged sections quickly without manually scrolling.
- This phase focuses on read efficiency and navigation speed, not editing behavior.

## Scope
- In scope:
  - Search-driven projected render mode per open file.
  - Projection built from current search query + mode (methods/types/tags/general chars).
  - Projection-to-real-line mapping so click/jump works reliably.
  - Scrollbar/line preview markers for all matches.
- Out of scope:
  - Editing inside projected mode (initially read-only mode).
  - Undo/redo model changes.
  - Full-text indexing across unopened files (current open files are enough for Phase C).

## Data model
Add a second render source for each open file:
- `EditorRenderSource`: `EDITOR_RENDER_REAL` or `EDITOR_RENDER_PROJECTION`.
- `SearchProjection`:
  - `char** lines` (projected display lines).
  - `int lineCount`.
  - `int* projectedToRealLine`.
  - `int* projectedToRealCol` (optional, for precise jump targets).
  - `int* realMatchLines` + count (for scrollbar markers).
  - `uint64_t buildStamp` (query+mode+buffer version).

Design rule:
- Never mutate `EditorBuffer` for projection.
- Projection is disposable and rebuilt when query/mode/file content changes.

## Implementation steps

1. Add projection state to editor file/view
- Files:
  - `src/ide/Panes/Editor/editor_view.h`
  - `src/ide/Panes/Editor/editor_view.c`
- Add projection structs and lifecycle helpers:
  - create/update/free projection.
  - enable/disable projection per active file based on search state.
- Status: Complete (Phase C.1)
- Completion doc:
  - `docs/basic_upgrades/completed/phase_c_1_projection_state_lifecycle.md`

2. Build projection generator from existing search mechanics
- Files:
  - `src/ide/Panes/ControlPanel/control_panel.c`
  - `src/ide/Panes/Editor/editor_projection.c`
  - `src/ide/Panes/Editor/editor_view.c`
- Reuse current search mode semantics (methods/types/tags/general char match).
- For each file, gather matched regions and emit projected lines with separators for readability.
- Store mapping from each projected line back to the source line.
- Status: Complete (Phase C.2)
- Completion doc:
  - `docs/basic_upgrades/completed/phase_c_2_projection_generator.md`

3. Switch editor render path by source mode
- Files:
  - `src/ide/Panes/Editor/Render/render_editor.c`
- If projection active, draw projected lines and highlights/underlines.
- Keep clip bounds, cursor visuals, and tab rendering unchanged.
- Show a subtle banner or label when projection mode is active.
- Status: Complete (Phase C.3)
- Completion doc:
  - `docs/basic_upgrades/completed/phase_c_3_projection_render_switch.md`

4. Add navigation mapping in input path
- Files:
  - `src/ide/Panes/Editor/Input/editor_input_mouse.c`
  - `src/ide/Panes/Editor/Input/editor_input_keyboard.c`
- In projection mode:
  - clicks select projected row, then map/jump cursor to real row.
  - Enter jumps to source line directly.
- Keep mode read-only so edits cannot target projected text accidentally.
- Status: Complete (Phase C.4)
- Completion doc:
  - `docs/basic_upgrades/completed/phase_c_4_projection_input_mapping_and_readonly.md`

5. Add scrollbar and in-pane preview markers
- Files:
  - `src/ide/Panes/Editor/Render/render_editor.c`
  - `src/ide/Panes/Editor/editor_view.c`
- If file content exceeds viewport height, draw match ticks on scrollbar track.
- If content fits in viewport, draw marker accents at actual line Y positions.
- Marker source is `realMatchLines`, independent of projection ordering.
- Status: Complete (Phase C.5)
- Completion doc:
  - `docs/basic_upgrades/completed/phase_c_5_match_markers_and_projection_scope.md`

6. Incremental rebuild + performance guardrails
- Files:
  - `src/ide/Panes/Editor/editor_projection.c`
  - `src/ide/Panes/Editor/Render/render_editor.c`
  - `src/ide/Panes/Editor/Input/editor_input_mouse.c`
  - `src/ide/Panes/Editor/editor_view.h`
- Rebuild projection only when:
  - query changes,
  - mode changes,
  - file buffer stamp changes.
- Add short debounce for rapid typing if rebuild cost becomes visible.
- Cap projection size and add `"... +N more matches"` lines for extreme files.
- Status: Complete (Phase C.6)
- Completion doc:
  - `docs/basic_upgrades/completed/phase_c_6_guardrails_and_projection_content_layout.md`

7. Control filter model refactor (compact targets/scopes/view modes)
- Files:
  - `src/ide/Panes/ControlPanel/control_panel.c`
  - `src/ide/Panes/ControlPanel/control_panel.h`
  - `src/ide/Panes/ControlPanel/render_control_panel.c`
  - `src/ide/Panes/ControlPanel/input_control_panel.c`
  - `src/ide/Panes/Editor/editor_view.c`
  - `src/ide/Panes/Editor/Render/render_editor.c`
- Plan doc:
  - `docs/basic_upgrades/phases/phase_c_7_control_filter_model_refactor.md`
- Status: Pending

## Acceptance criteria
- Enabling search shows projected results in the editor without modifying source buffers.
- Clicking projected lines reliably jumps to true source lines.
- Match markers appear in scrollbar or in-place line positions depending on file height.
- Search mode remains responsive while typing and while switching open tabs/files.

## Validation checklist
- Query for method prefix: projected view shows only relevant methods/contexts.
- Query for type/tag mode: projected results follow the selected mode semantics.
- Switching tabs updates projection correctly for each file.
- Clearing search returns normal full-file render immediately.
- No regressions in save, undo/redo, or normal text editing when projection mode is off.

## Completion notes template
- What was shipped.
- Remaining gaps deferred to the next C sub-step.
- Perf observations and follow-up thresholds.
