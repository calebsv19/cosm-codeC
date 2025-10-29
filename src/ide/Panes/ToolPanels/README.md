# Tool Panels

Side panes that surface project metadata (files, build results, errors, etc.).
Each specific tool lives in its own subdirectory while the common chrome is
handled by the base files in this folder.

| Shared File | Responsibility |
| --- | --- |
| `render_tool_panel.h/c` | Draws the shared chrome (headers, tabs) for tool panels. |
| `input_tool_panel.h/c` | Routes mouse/keyboard events that affect the shared chrome. |
| `command_tool_panel.h/c` | Dispatches high-level commands that apply to whichever tool is active. |

## Specialized Tools

- [`Project/`](Project/README.md) — Project tree, file management, drag-and-drop into editor views.
- [`Tasks/`](Tasks/README.md) — Simple task list with keyboard shortcuts for task CRUD.
- [`Libraries/`](Libraries/README.md) — Placeholder for library inspector (currently minimal).
- [`BuildOutput/`](BuildOutput/README.md) — Displays compiler output and provides clear/scroll commands.
- [`Errors/`](Errors/README.md) — Future error list pane (stub for now).
- [`Assets/`](Assets/README.md) — Asset browser placeholder.
- [`Git/`](Git/README.md) — Git status view stub.
