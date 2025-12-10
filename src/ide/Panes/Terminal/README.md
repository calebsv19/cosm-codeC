# Terminal Pane

PTY-backed terminal pane for running a real shell plus dedicated build/run
sessions.

| File | Responsibility |
| --- | --- |
| `terminal.h/c` | PTY spawn (forkpty), shell lifecycle, multi-session list (interactive/build/run), scrollback/grid. |
| `render_terminal.h/c` | Renders the terminal grid, cursor, and handles resize → SIGWINCH. |
| `input_terminal.h/c` | Keyboard mapping (arrows, ctrl keys, backspace), focus, scroll. |
| `command_terminal.h/c` | Command bus events (spawn, destroy, focus). |

Build and run buttons target their own terminals; output also feeds the build
output panel for structured errors/warnings.
