# IDE Front-End

This branch contains everything the user sees: pane definitions, layout/UI
helpers, and the in-process plugin boundary. The structure mirrors the runtime
layout:

| Subdirectory | Highlights |
| --- | --- |
| [`UI/`](UI/README.md) | Global layout engine, pane geometry helpers, and shared UI utilities. |
| [`Panes/`](Panes/README.md) | Each concrete pane (editor, menu bar, tool panels, etc.) with dedicated command, render, and input files. |
| [`Plugin/`](Plugin/README.md) | Explicit opt-in plugin loader. Plugins are in-process, allowlist-gated, and treated as trusted code rather than sandboxed extensions. |

Use this tree when you need to change how the IDE looks or behaves at the pane
level; logic that should remain headless belongs in `core/`.
