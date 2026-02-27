# Build & Run Helpers

This module wraps build and run orchestration so panes can trigger project work
without embedding process-management logic.

| File | Responsibility |
| --- | --- |
| `build_system.h/c` | High-level orchestration: triggers builds, tracks `BuildStatus`, stores the output log, and records the last resolved executable path. |
| `build_diagnostics.h/c` | Incrementally parses compiler output into structured file/line diagnostics and persists those diagnostics per workspace. |
| `run_build.h/c` | Launches the selected executable and streams stdout/stderr back into the IDE's terminal-facing surfaces. |

The code deliberately avoids pane knowledge. Callers ask for a build or run,
then render the resulting status/logs wherever they need.

## Current behaviour

- By default the IDE runs `make` in the workspace root and looks for the newest
  executable under `<workspace>/build/`. You can still override the active run
  target from the project tree.
- Build and run commands can be customised via `~/.custom_c_ide/config.ini`.
  Supported keys are:
  - `build_command`, `build_args`, `build_workdir`, `build_output_dir`
  - `run_command`, `run_args`, `run_workdir`
  Blank values fall back to the default behaviour above.
- When a custom run command is provided you can reference the active run target
  using `{TARGET}` inside `run_args`. If the token is absent the path is appended
  automatically as the final argument so existing workflows continue to work.
- Build output and parsed diagnostics are persisted under `ide_files/` so the
  terminal and build-output views can restore useful context on relaunch.
- Security hardening removed the old "shell string" assumption from the IPC
  build path. Configuration values should be treated as argv-style command data,
  not shell pipelines with implicit redirection or pipes.
