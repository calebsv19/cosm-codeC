# Global Information Singletons

Everything in this directory describes or manages the globally accessible
state used by the IDE. Other modules include these headers to observe or
mutate shared state without having to know who owns the memory.

| File | Responsibility |
| --- | --- |
| `core_state.h/c` | Defines `IDECoreState` (focused pane, active editor view, drag state, timers, etc.) and the helper functions that expose a single instance via `getCoreState()`. |
| `event_loop.h/c` | Implements the main frame loop: polling SDL events, routing input through the `core/InputManager`, ticking background systems (command bus, file watcher), and kicking off rendering. |
| `project.h/c` | Tracks the currently loaded project root, scans the filesystem into `DirEntry` trees, and exposes helper functions for project refreshes. |
| `system_control.h/c` | Thin façade for bootstrapping and tearing down SDL subsystems, renderer assets, and other one-off runtime services. |

Together these modules act as the boundary between process-level wiring in
`app/` and the reusable systems in `core/` and `ide/`.
