# Phase C.6 Completion: Guardrails + Projection Content Layout

## Goal
Complete Phase C guardrails and improve projection readability:
- add projection size caps
- render full method bodies for matched methods
- add stable line-number gutter in editor views
- keep projection results readable without inline numeric prefixes

## Shipped
- Guardrails in projection generator (`src/ide/Panes/Editor/editor_projection.c`):
  - max projection row cap (`PROJECTION_MAX_ROWS`)
  - function body cap (`PROJECTION_MAX_FUNCTION_LINES`)
  - overflow summary row: `... +N more projected rows`
- Projection content layout changes (`src/ide/Panes/Editor/editor_projection.c`):
  - matched methods now render as full method blocks (start to end line, capped)
  - non-method symbol matches still render targeted source lines
  - plain text matches render full source lines
  - removed inline `NNNN |` prefixes so renderer owns line numbering
- Editor line-number gutter for all views (`src/ide/Panes/Editor/Render/render_editor.c`):
  - added fixed left gutter rendering
  - line numbers drawn for real mode and mapped projection rows
  - separator line between gutter and content
- Input alignment update for new gutter (`src/ide/Panes/Editor/Input/editor_input_mouse.c`):
  - click/cursor text origin moved to account for gutter width
- Shared gutter constant:
  - `EDITOR_LINE_NUMBER_GUTTER_W` in `src/ide/Panes/Editor/editor_view.h`

## Done Criteria Check
- Projection rebuild remains stamp-based and disposable: yes.
- Projection output now favors full method readability: yes.
- Large projections are capped with explicit overflow feedback: yes.
- Line numbers are rendered by editor UI (not embedded in projected text): yes.
- Build passes: yes.

## Deferred Follow-Ups
- Optional debounce timer for large all-open projection fan-out.
- Optional user-facing toggle for function-body expansion depth.
