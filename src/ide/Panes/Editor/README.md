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
| `editor_live_diagnostics.h/c` | Captures debounced active-buffer snapshots and enqueues background live diagnostics when Live Parse is enabled. |
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

`editor_header_controls.c` borrows painted close/tab controls per split leaf.
Input accepts matching release only while the file order, active tab, tab scroll,
workspace and geometry snapshot remains current. View/tab retirement cancels
borrowed targets before reuse. Input retains the existing command bus for close
and state synchronization for tab selection. Keyboard/document and continuous
scroll/selection/split gestures stay on their existing paths.

Final capacity review (2026-10-07): existing sessions may have more than sixteen
open files. Header snapshots therefore retain every live file identity, while
visible controls stay bounded by the shared scope capacity. An 80-tab fixture
proves late-index selection/close and cancellation after an off-screen file
changes. Eight split leaves retain independent scopes. The corrected final
snapshot passes the full contained and native verification gates. Current native
font presets display missing glyph placeholders for some Chinese/emoji text;
UTF-8 editing/retention passes, but glyph coverage and human IME acceptance remain
open visual qualification work. Source VERSION remains unchanged; the release
impact dry-run recommends a minor release for eventual public adoption.

The prior field adapter caret used an unmapped SDL floating-rectangle call. The
final painter uses the actual Vulkan-compatible rectangle call, and the Vulkan
contract now rejects unmapped SDL calls in native UI adapters. A negative fixture
confirms rejection; final native rename captures force the caret visible for
render review while ordinary blink policy is retained.

Rename blink timestamps and animation advancement now use the same SDL time
origin; the previous mixed-clock call could hide the caret immediately.
Deterministic blink-transition checks pass, and the opt-in native fixture
confirms a visible caret. Ordinary blink intervals retain their existing policy.

Native field/button captions, terminal labels and rename popup sizing now use
the existing UTF-8 width helper rather than byte-oriented text measurement,
matching the UTF-8 drawing path. Font fallback remains separately unqualified.
