# Errors Tool Panel

Diagnostics panel showing analysis errors/warnings grouped per file. Supports
collapse/expand, scrolling, multi-select/copy, and double-click to jump to
file/line.

| File | Responsibility |
| --- | --- |
| `tool_errors.h/c` | Stores flattened diagnostics, selection state, collapse flags, jump-to-file. |
| `render_tool_errors.h/c` | Renders grouped file headers + messages with clip/scroll and scrollbar. |
| `input_tool_errors.h/c` | Mouse/scroll input (wheel + thumb drag), selection ranges, double-click navigation. |
| `command_tool_errors.h/c` | Command bus glue (minimal today). |

Fed by `core/Analysis` diagnostics; build output panel is separate for compiler logs.
