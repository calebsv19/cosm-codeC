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

- By default the IDE simply runs `make` in the workspace root and looks for the
  newest executable under `<workspace>/build/`. You can still pick a different
  target by selecting it in the project tree.
- Build and run commands can be customised via
  `~/.custom_c_ide/config.ini`. Supported keys:
  - `build_command`, `build_args`, `build_workdir`, `build_output_dir`
  - `run_command`, `run_args`, `run_workdir`
  Blank values fall back to the default behaviour above.
- When a custom run command is provided you can reference the active run target
  using `{TARGET}` inside `run_args`. If the token is absent the path is appended
  automatically as the final argument so existing workflows continue to work.
- Build success/failure output, including the resolved working directory,
  command, and any configured artifact directory, is streamed to the terminal
  pane for transparency.
