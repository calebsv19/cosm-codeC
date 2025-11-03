# Build & Run Helpers

This module wraps project build tooling so panes (menu bar, task panes, etc.)
can trigger builds without knowing about the underlying shell commands.

| File | Responsibility |
| --- | --- |
| `build_system.h/c` | High-level orchestration: queues build requests, tracks state, and exposes convenience functions such as `triggerBuild()`. |
| `run_build.h/c` | Platform-specific helpers that spawn the compiled executable, stream stdout/stderr into the terminal pane, and manage process lifetime. |

The code deliberately avoids UI knowledge—caller panes simply receive callbacks
and render whatever status they need.

## Current behaviour

- Builds are executed from the active workspace root. Artifacts land in
  `<workspace>/BuildOutputs/<target>/` and the module records the newest
  executable produced during the run.
- If the user has not picked a run target yet, the latest built executable is
  automatically registered and persisted so the Run command can execute it
  immediately. Selections made in the project pane override this.
- Build success/failure output, including the resolved executable path, is
  streamed to the terminal pane for transparency.*** End Patch
