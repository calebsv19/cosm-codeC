# Libraries Tool Panel

Placeholder pane intended to surface third-party dependencies. Currently very
lightweight but wired into the standard pane pattern so it can be expanded.

| File | Responsibility |
| --- | --- |
| `tool_libraries.h/c` | Holds placeholder data structures and initialises the pane. |
| `render_tool_libraries.h/c` | Renders whatever library list is available (simple text list today). |
| `input_tool_libraries.h/c` | Handles basic focus/selection input (stub). |
| `command_tool_libraries.h/c` | Command bus handlers (currently no-ops, ready for future expansion). |

Iterate here when turning the library inspector into a full feature.
