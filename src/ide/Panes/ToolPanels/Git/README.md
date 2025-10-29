# Git Tool Panel

Stub UI for visualising Git status. Implements the same pane skeleton so it
can grow into a fully-featured view.

| File | Responsibility |
| --- | --- |
| `tool_git.h/c` | Pane initialisation and placeholder data wiring. |
| `render_tool_git.h/c` | Renders the current Git status tree (placeholder). |
| `input_git.c` / `input_tool_git.h` | Mouse/keyboard handlers (minimal). |
| `command_tool_git.h/c` | Command bus actions (refresh, stage, commit – currently stubs). |
| `tree_git_adapter.h/c` | Helper intended to map Git status entries into the shared tree renderer. |

Fill in these stubs when Git integration moves beyond the prototype stage.
