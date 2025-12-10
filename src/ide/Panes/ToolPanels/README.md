# Tool Panels

Side panes that surface project metadata (files, build results, errors, etc.).
Each specific tool lives in its own subdirectory while the common chrome is
handled by the base files in this folder.

| Shared File | Responsibility |
| --- | --- |
| `render_tool_panel.h/c` | Draws the shared chrome (headers, tabs) for tool panels. |
| `input_tool_panel.h/c` | Routes mouse/keyboard events that affect the shared chrome. |
| `command_tool_panel.h/c` | Dispatches high-level commands that apply to whichever tool is active. |

## Specialized Tools (current)

- [`Project/`](Project/README.md) — Project tree, file management, drag-and-drop into editor views.
- [`Tasks/`](Tasks/README.md) — Task list with keyboard shortcuts for task CRUD.
- [`Libraries/`](Libraries/README.md) — Placeholder for library inspector (currently minimal).
- [`BuildOutput/`](BuildOutput/README.md) — Streams build terminal output and parsed errors/warnings.
- [`Errors/`](Errors/README.md) — Analysis diagnostics grouped per file, multi-select, jump-to-file.
- [`Assets/`](Assets/README.md) — Asset browser grouped by type (images/audio/data/other), scrollable, text-like files open in editor.
- [`Git/`](Git/README.md) — Git changes grouped by status plus collapsible git log, scrollable/selection-aware.
