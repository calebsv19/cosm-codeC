# Build Output Tool Panel

Displays compiler/build logs streamed from the build system.

| File | Responsibility |
| --- | --- |
| `tool_build_output.h/c` | Manages the build output buffer and scroll state. |
| `build_output_panel_state.h/c` | Helper struct for tracking scrollback and view offsets independent of the pane instance. |
| `render_tool_build_output.h/c` | Draws the log lines and scroll indicators. |
| `input_tool_build_output.h/c` | Handles scroll wheel and keyboard shortcuts. |
| `command_tool_build_output.h/c` | Executes commands such as clearing the log or jumping to the top. |

Logs are populated by `core/BuildSystem/run_build`, which pushes text straight
into the buffer managed here.
