# Global Information Singletons

Everything in this directory describes or manages the globally accessible
state used by the IDE. Other modules include these headers to observe or
mutate shared state without having to know who owns the memory.

| File | Responsibility |
| --- | --- |
| `core_state.h/c` | Defines `IDECoreState` (focused pane, active editor view, drag state, timers, etc.) and the helper functions that expose a single instance via `getCoreState()`. |
| `event_loop.h/c` | Implements the main frame loop: polling SDL events, routing input through the `core/InputManager`, ticking background systems (command bus, file watcher), and kicking off rendering. |
| `project.h/c` | Tracks the currently loaded project root, scans the filesystem into `DirEntry` trees, and exposes helper functions for project refreshes. |
| `startup_diagnostics.h/c` | Captures the R3 startup diagnostics snapshot for runtime resource-root selection, workspace startup selection, and IPC availability. The optional `IDE_STARTUP_DIAG_LOG=1` print omits IPC auth tokens. |
| `system_control.h/c` | Thin façade for bootstrapping and tearing down SDL subsystems, renderer assets, and other one-off runtime services. |
| `visual_artifact_proof.h/c` | Env-gated one-shot visual artifact capture used by the R6 proof target. |
| `workspace_context.h/c` | Captures a read-only snapshot of project/workspace/root/build context for subsystems that need path context without directly reaching across multiple singleton owners. |
| `workspace_startup_policy.h/c` | Pure helpers for choosing the default workspace root and the stored-versus-fallback startup selection policy. |

Together these modules act as the boundary between process-level wiring in
`app/` and the reusable systems in `core/` and `ide/`.
