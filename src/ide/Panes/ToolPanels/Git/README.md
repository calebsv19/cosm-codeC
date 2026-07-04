# Git Tool Panel

Git panel showing status + git log in a shared tree renderer. Sections are
collapsible; entries are scrollable and selectable (actions are still minimal).

| File | Responsibility |
| --- | --- |
| `tool_git.h/c` | Fetches git status and streams git log history into a dynamic model. |
| `git_command_runner.h/c` | Runs Git panel commands through argv-based `fork`/`execvp` with child-side `chdir`, avoiding shell-mediated workspace paths. |
| `render_tool_git.h/c` | Builds a tree with “Changes” (grouped by status) and “Log” (full streamed history), renders with scroll. |
| `input_git.c` / `input_tool_git.h` | Mouse/scroll handlers; scroll-aware selection, thumb drag. |
| `command_tool_git.h/c` | Command bus actions (refresh; stage/commit still stubbed). |
| `tree_git_adapter.h/c` | Converts git model into UITreeNode hierarchy. |

## Log Loading Controls

- `IDE_GIT_LOG_PAGE_SIZE`: commits parsed per UI pump tick (default `200`, bounds `20-5000`).
- `IDE_GIT_LOG_MAX_COMMITS`: optional hard ceiling for loaded commits (`0` or unset means full history).

Future: stage/unstage, commit, diff previews, clickable log details.

## Command Execution Boundary

Git status, branch, log, stage, and commit commands are launched through
`git_command_runner.*` using explicit argv arrays and child-side `chdir` into
the selected project path. The panel does not interpolate the project path,
commit message, or log arguments into shell command strings. Status/log watcher
queries suppress stderr; stage and commit capture the first stdout/stderr line
for the panel status text.
