# Tasks Tool Panel

Lightweight task tracker integrated into the IDE.

| File | Responsibility |
| --- | --- |
| `tool_tasks.h/c` | Owns the task list data structures and exposes helpers for CRUD operations. |
| `render_tool_tasks.h/c` | Draws the task list, expanded/collapsed states, and completion indicators. |
| `input_tool_tasks.h/c` | Keyboard shortcuts (`Ctrl+N`, `Ctrl+D`, etc.) and mouse selection handling. |
| `command_tool_tasks.h/c` | Command bus handlers for task operations (add, delete, rename, move). |
| `task_json_helper.h/c` | Serialisation helpers for saving and loading task lists (JSON). |

Future enhancements (sorting, filters) should build on the structures here so
they remain command-driven.
