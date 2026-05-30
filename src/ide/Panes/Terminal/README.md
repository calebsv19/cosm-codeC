# Terminal Pane

PTY-backed terminal pane for running a real shell plus dedicated build/run
sessions.

| File | Responsibility |
| --- | --- |
| `terminal.h/c` | Public terminal facade, PTY/session orchestration, resize/runtime hooks, and shared terminal globals. |
| `terminal_internal.h` | Private terminal-session contract shared across extracted terminal core modules. |
| `terminal_grid.h/c` | Terminal grid write path, UTF-8/codepoint handling, escape-sequence parsing, and alternate-screen mode routing. |
| `terminal_grid_internal.h` | Private grid buffer/state helpers shared across extracted terminal grid modules. |
| `terminal_grid_buffer.c` | Grid allocation, resize preservation, scrollback storage, viewport sizing, and core buffer lifecycle helpers. |
| `terminal_session_model.c` | Session projection/model rebuilding, journal snapshot capture, scroll-state helpers, and projection accessors. |
| `terminal_session_registry.c` | Session/task registry, tab-hit rectangles, creation/close flows, and non-backend session mutation helpers. |
| `terminal_session_runtime.c` | PTY output pump, shell lifecycle, dropped-path send flow, runtime clear/print helpers, font-metric invalidation, and pane-resize handling. |
| `render_terminal.h/c` | Renders the terminal grid, cursor, and handles resize → SIGWINCH. |
| `input_terminal.h/c` | Keyboard mapping (arrows, ctrl keys, backspace), focus, scroll. |
| `command_terminal.h/c` | Command bus events (spawn, destroy, focus). |

Build and run buttons target their own terminals; output also feeds the build
output panel for structured errors/warnings.
