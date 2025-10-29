# Pane Implementations

Each folder under `Panes/` corresponds to a pane shown in the IDE window. The
pattern is consistent: panes generally expose `render_*.c`, `input_*.c`, and
`command_*.c` files so rendering, input collection, and command execution stay
separate.

| Pane | Purpose |
| --- | --- |
| [`ControlPanel/`](ControlPanel/README.md) | Houses toggles for diagnostics, live parse flags, and other global controls. |
| [`Editor/`](Editor/README.md) | Text editor panes (buffer management, text rendering, undo stack, command handlers). |
| [`IconBar/`](IconBar/README.md) | Sidebar of quick-access icons that change the active tool panels. |
| [`MenuBar/`](MenuBar/README.md) | Top menu bar with build/run commands and global toggles. |
| [`PaneInfo/`](PaneInfo/README.md) | Shared definitions for pane structs, roles, and helpers used by all panes. |
| [`Popup/`](Popup/README.md) | Modal overlay pane for confirmations, rename prompts, etc. |
| [`Terminal/`](Terminal/README.md) | Integrated terminal and build output view. |
| [`ToolPanels/`](ToolPanels/README.md) | Collection of auxiliary panes (Project browser, Tasks, Git, etc.). |

See each subdirectory README for file-level details.
