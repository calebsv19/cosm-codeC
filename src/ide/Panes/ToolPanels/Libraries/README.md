# Libraries Tool Panel

This pane surfaces the workspace's include/library picture using the analysis
pipeline's persisted library index.

| File | Responsibility |
| --- | --- |
| `tool_libraries.h/c` | Owns the flattened row model, selection state, expand/collapse state, scroll state, and header toggle state for the panel. |
| `render_tool_libraries.h/c` | Renders bucket rows, header rows, usage rows, and the panel-specific controls. |
| `input_tool_libraries.h/c` | Handles selection, drag-select, row clicks, and header interactions. |
| `command_tool_libraries.h/c` | Exposes explicit commands to refresh the library index and clear analysis cache before forcing a rebuild. |

The UI is populated from [`core/Analysis/library_index`](../../../../core/Analysis/README.md)
and groups data into buckets (project, local, system, unresolved), then expands
down to individual headers and usage sites. This is no longer a placeholder pane.
