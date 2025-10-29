# Editor Pane

Implements the multi-view text editor: buffer management, rendering, command
execution, and state persistence. The code is split into logical layers so we
can reuse the editing core without the SDL UI if needed.

## Core Modules

| File | Responsibility |
| --- | --- |
| `editor.h/c` | High-level helpers used by other panes (insert/delete at cursor, clipboard helpers). |
| `editor_core.h/c` | Constructs editor panes, manages editor views and tabs. |
| `editor_view.h/c` | Tree representing splits/tabs plus helpers to locate active views, open files, etc. |
| `editor_view_state.h/c` | Transient state (dragging tabs, hover targets) used by the view layout/render path. |
| `editor_buffer.h/c` | Owns text storage for an open file (array of lines, capacity management). |
| `editor_state.h/c` | Cursor position, selection info, viewport offsets per open file. |
| `editor_text_edit.h/c` | Implements editing primitives (insert char, newline, delete forward/backwards) with undo hooks. |
| `editor_clipboard.h/c` | Cut/copy/paste helpers that operate on selections. |
| `buffer_safety.h/c` | Guard rails that keep buffers valid (e.g. ensure at least one empty line after deletes). |
| `undo_stack.h/c` | Undo/redo storage and operations for editor buffers. |

## Subdirectories

- [`Commands/`](Commands/README.md) – Command bus handlers and per-command helpers.
- [`Input/`](Input/README.md) – Keyboard/mouse/TextInput event routing for editor panes.
- [`Render/`](Render/README.md) – Drawing primitives for editor views (text, cursors, selections).

This structure keeps editing logic reusable while the pane files glue it to the
rest of the IDE.
