# Assets Tool Panel

Explorer for project assets (textures, audio, data, other). Scans the current
workspace, groups by type, supports collapse/scroll, and opens text-like files
in the editor.

| File | Responsibility |
| --- | --- |
| `tool_assets.h/c` | Scans workspace (skips .git/ide_files/build), classifies assets, stores catalog. |
| `render_tool_assets.h/c` | Renders grouped lists with clip + scrollbar, selection highlight, count caps. |
| `input_tool_assets.h/c` | Mouse/scroll input; double-click to open text-like assets. |
| `command_tool_assets.h/c` | Refresh hooks (currently minimal). |

Future: image/audio preview hooks, per-bucket scrolling, file watcher refresh.
