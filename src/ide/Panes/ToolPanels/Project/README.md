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

## Build output integration

The BuildOutputs directory for the active workspace is rendered directly in the
tree so users can treat build artefacts like any other file. Selecting a file
inside `BuildOutputs/` marks it as the run target (entries are tinted red) and
the choice is persisted in `~/.custom_c_ide/config.ini` alongside the workspace.
If a folder inside `BuildOutputs/` is selected instead, the newest executable in
that folder is used. Builds automatically refresh the tree and, when no explicit
target has been chosen yet, the most recent artefact becomes the new run target.*** End Patch
