# Control Panel Pane

The control panel provides project-wide toggles (live parse, show errors, etc.)
and demonstrates the typical pane structure used across the IDE.

| File | Responsibility |
| --- | --- |
| `control_panel.h/c` | Pane construction and shared state helpers. |
| `render_control_panel.h/c` | Draws the control panel UI (buttons, switches, status labels). |
| `input_control_panel.h/c` | Captures keyboard and mouse input inside the pane; enqueues commands using the command bus. |
| `command_control_panel.h/c` | Executes commands destined for this pane (toggling UI flags, invoking diagnostics). |

Follow this pattern when introducing new panes—keep rendering, input, and
command handling isolated for clarity.
