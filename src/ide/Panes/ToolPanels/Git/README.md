# Git Tool Panel

Git panel showing status + git log in a shared tree renderer. Sections are
collapsible; entries are scrollable and selectable (actions are still minimal).

| File | Responsibility |
| --- | --- |
| `tool_git.h/c` | Fetches git status/log, holds model arrays. |
| `render_tool_git.h/c` | Builds a tree with “Changes” (grouped by status) and “Log” (recent commits), renders with scroll. |
| `input_git.c` / `input_tool_git.h` | Mouse/scroll handlers; scroll-aware selection, thumb drag. |
| `command_tool_git.h/c` | Command bus actions (refresh; stage/commit still stubbed). |
| `tree_git_adapter.h/c` | Converts git model into UITreeNode hierarchy. |

Future: stage/unstage, commit, diff previews, clickable log details.
