# Errors Tool Panel

Diagnostics panel showing analysis errors/warnings grouped per file. Supports
collapse/expand, scrolling, multi-select/copy, selected-diagnostic detail,
subtle units/dimension detail when diagnostics provide context metadata,
panel-local search, and double-click/keyboard jump to file/line.

| File | Responsibility |
| --- | --- |
| `tool_errors.h/c` | Stores flattened diagnostics, selection/search state, collapse flags, jump-to-file. |
| `render_tool_errors.h/c` | Renders grouped file headers + messages with clip/scroll and scrollbar, including the compact analysis status badge for updating and queued save diagnostics. |
| `input_tool_errors.h/c` | Mouse/scroll input (wheel + thumb drag), selection ranges, double-click navigation. |
| `command_tool_errors.h/c` | Command bus glue (minimal today). |
| `errors_filter.h/c` | Panel-local diagnostic query matching over message/file/hint/category/code/stage. |
| `errors_units_detail.h/c` | Extracts compact units/dimension detail from diagnostic context JSON and optional symbol units attachments. |
| `errors_context_detail.h/c` | Extracts compact include/macro/details rows and navigation targets from diagnostic context JSON. |
| `errors_diagnostic_detail.h/c` | Builds the selected-diagnostic detail line model consumed by the renderer. |

Fed by `core/Analysis` diagnostics; build output panel is separate for compiler logs.
