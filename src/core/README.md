# Core Runtime Systems

Code under `core/` holds reusable subsystems that do not depend on any specific
pane or UI. These modules can be embedded in other front ends or tested in
isolation.

| Subdirectory | What it provides |
| --- | --- |
| [`BuildSystem/`](BuildSystem/README.md) | Helpers for triggering project builds, running executables, and reporting build status back to the IDE. |
| [`CommandBus/`](CommandBus/README.md) | Central command queue, metadata definitions, and validators that decouple input events from pane actions. |
| [`Diagnostics/`](Diagnostics/README.md) | Hooks for future profiling or error reporting (currently stubs). |
| [`FileIO/`](FileIO/README.md) | Small filesystem utilities shared by multiple panes and systems. |
| [`InputManager/`](InputManager/README.md) | SDL input routing, per-device handlers, and high-level command generation. |
| [`Watcher/`](Watcher/README.md) | File-watcher loop that keeps the project tree in sync with disk changes. |

Each subfolder contains a README with more detailed file summaries.
