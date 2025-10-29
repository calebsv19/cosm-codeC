# Icon Bar Pane

The icon bar is a narrow vertical strip with shortcuts for switching tool
panels. Its structure mirrors other panes.

| File | Responsibility |
| --- | --- |
| `icon_bar.h/c` | Pane creation plus shared state (active icon, hover state). |
| `render_icon_bar.h/c` | Draws icon buttons and hover/focus indicators. |
| `input_icon_bar.h/c` | Handles mouse clicks and keyboard shortcuts (`Ctrl+1…7`) that map to icons. |
| `command_icon_bar.h/c` | Implements the icon-selection commands the bus dispatches. |

Use this pane as an example when building other simple button matrices.
