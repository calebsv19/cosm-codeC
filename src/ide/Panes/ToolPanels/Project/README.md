# Project Tool Panel

File explorer pane with support for opening files, creating/deleting entries,
drag-and-drop into editor views, and rename workflows.

| File | Responsibility |
| --- | --- |
| `tool_project.h/c` | Core state (selected file/folder, hover tracking), directory interaction helpers, and deletion logic. |
| `render_tool_project.h/c` | Renders the tree view using the shared `UI/Trees` helper plus the toolbar buttons (add/delete file/folder). |
| `input_tool_project.h/c` | Mouse/keyboard handling: toolbar clicks, drag initiation, rename shortcuts, Ctrl-based hotkeys. |
| `command_tool_project.h/c` | Executes project-specific commands from the command bus (open file, rename, create/delete entries). |
| `rename_callbacks.h/c` | Glue between the generic rename flow and the filesystem (renaming files/folders on disk and refreshing the project tree). |

This pane coordinates closely with `app/GlobalInfo/project.c` (which scans the
filesystem) and `core/CommandBus` for command dispatch.
