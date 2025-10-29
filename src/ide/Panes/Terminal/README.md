# Terminal Pane

Embedded terminal/build output panel for streaming logs from the build system
or external processes.

| File | Responsibility |
| --- | --- |
| `terminal.h/c` | Pane construction, ring buffer for terminal lines, and helper functions to push text. |
| `render_terminal.h/c` | Renders the scrollback buffer and status line. |
| `input_terminal.h/c` | Handles keyboard shortcuts (clear, run executable) and mouse scroll. |
| `command_terminal.h/c` | Executes command bus events targeting the terminal (clear, run). |

The terminal collaborates with `core/BuildSystem/run_build` to stream process
output directly into the pane.
