# Phase C.3 Completion: Projection Render Switch

## Goal
Render projected rows in editor views when projection mode is active, while preserving default rendering when projection mode is off.

## Shipped
- Added render-source helpers in `src/ide/Panes/Editor/Render/render_editor.c`:
  - projection source activation check
  - active render line count helper
  - active render line text helper
- Switched `renderEditorBuffer(...)` to source-aware rendering:
  - new signature takes `OpenFile*`
  - draws projection lines when projection mode is active
  - keeps selection registration and cursor rendering only for real buffer mode
  - keeps clipping behavior unchanged
- Updated `renderEditorScrollbar(...)` to size/scroll against active source line count.
- Added subtle projection badge (`projection view`) above content area when projection mode is active.
- Updated declaration in `src/ide/Panes/Editor/Render/render_editor.h`.

## Done Criteria Check
- Active search state shows projected rows in editor: yes.
- Clearing search returns normal real-buffer render: yes.
- Scrollbar range tracks active render source: yes.
- Build passes: yes.

## Deferred To Next Step
- Projection input mapping (click/enter to jump to real source lines).
- Explicit read-only behavior for projection mode in input path.
- Match marker rendering on scrollbar/in-pane.
