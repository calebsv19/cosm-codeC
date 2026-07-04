# Control Panel Pane

The control panel provides project-wide toggles, symbol search, unit attachment
search, editor projection controls, and analysis status.

| File | Responsibility |
| --- | --- |
| `control_panel.h/c` | Pane construction and shared state helpers. |
| `render_control_panel.h/c` | Draws the control panel UI (buttons, switches, status labels). |
| `input_control_panel.h/c` | Captures keyboard and mouse input inside the pane; enqueues commands using the command bus. |
| `command_control_panel.h/c` | Executes commands destined for this pane (toggling UI flags, invoking diagnostics). |
| `symbol_tree_adapter.h/c` | Builds and filters the declaration symbol tree. |
| `control_panel_search_filters_helpers.c` | Builds search/filter options and the read-only projection/search snapshot consumed by editor projection, marker, and live-diagnostics code. |
| `control_panel_units_tree.h/c` | Builds and filters the unit-attachment tree for the `Units` target. |

Follow this pattern when introducing new panes—keep rendering, input, and
command handling isolated for clarity.
