# Assets Tool Panel

Explorer for project assets (textures, audio, etc.). Currently a stub but
shares the common tool-panel architecture so it can be fleshed out later.

| File | Responsibility |
| --- | --- |
| `tool_assets.h/c` | Pane setup and placeholder data for asset entries. |
| `render_tool_assets.h/c` | Renders the asset list. (Note: an accidental `render_tool_assets.c'` duplicate exists; planned clean-up will remove it.) |
| `input_tool_assets.h/c` | Handles keyboard/mouse navigation (stub). |
| `command_tool_assets.h/c` | Command bus actions like refresh/preview (currently minimal). |

Once the asset browser is feature-complete this README should be updated with
the real data flow.
