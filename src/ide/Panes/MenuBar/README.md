# Menu Bar Pane

Provides the top-of-window command strip (File/Build/Run toggles, pane
visibility switches).

| File | Responsibility |
| --- | --- |
| `menu_buttons.h/c` | Declarative list of buttons and helper to map mouse positions to button IDs. |
| `render_menu_bar.h/c` | Draws button backgrounds, text labels, and hover states. |
| `input_menu_bar.h/c` | Mouse and keyboard handling (`Ctrl+B`, `Ctrl+R`, etc.) that map to command bus events. |
| `command_menu_bar.h/c` | Executes the command bus actions (build triggers, toggling control/tool panels, queuing saves). |

Because the menu bar orchestrates other panes, it depends on `core/BuildSystem`
and `core/CommandBus` but avoids editor-specific knowledge.
