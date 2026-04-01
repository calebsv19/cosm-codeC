# Git Tool Panel

Git panel showing status + git log in a shared tree renderer. Sections are
collapsible; entries are scrollable and selectable (actions are still minimal).

| File | Responsibility |
| --- | --- |
| `tool_git.h/c` | Fetches git status and streams git log history into a dynamic model. |
| `render_tool_git.h/c` | Builds a tree with “Changes” (grouped by status) and “Log” (full streamed history), renders with scroll. |
| `input_git.c` / `input_tool_git.h` | Mouse/scroll handlers; scroll-aware selection, thumb drag. |
| `command_tool_git.h/c` | Command bus actions (refresh; stage/commit still stubbed). |
| `tree_git_adapter.h/c` | Converts git model into UITreeNode hierarchy. |

## Log Loading Controls

- `IDE_GIT_LOG_PAGE_SIZE`: commits parsed per UI pump tick (default `200`, bounds `20-5000`).
- `IDE_GIT_LOG_MAX_COMMITS`: optional hard ceiling for loaded commits (`0` or unset means full history).

Future: stage/unstage, commit, diff previews, clickable log details.
