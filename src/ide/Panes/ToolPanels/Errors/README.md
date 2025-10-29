# Errors Tool Panel

Placeholder panel intended to surface compiler or diagnostics errors. The
structure matches other tool panels so future work can drop in real data.

| File | Responsibility |
| --- | --- |
| `tool_errors.h/c` | Initialises the pane and holds stub data structures for error entries. |
| `render_tool_errors.h/c` | Renders placeholder text (ready to render real error lists). |
| `input_tool_errors.h/c` | Handles focus/scroll input (currently minimal). |
| `command_tool_errors.h/c` | Command bus glue for future actions (e.g. jump to error). |

When diagnostics land in `core/Diagnostics`, channel them through this pane.
