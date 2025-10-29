# Editor Input Handlers

Editor panes listen for several SDL event types. These handlers translate raw
events into command bus messages or direct buffer interactions (for latency-
sensitive cursor movement/text entry).

| File | Responsibility |
| --- | --- |
| `input_editor.h/c` | Registers the pane’s input callbacks and contains high-level helpers shared by the keyboard/mouse paths. |
| `editor_input_keyboard.h/c` | Consumes `COMMAND_EDITOR_KEYDOWN` payloads, manages selection state, and converts modifier-aware shortcuts into editing operations. |
| `editor_input_mouse.h/c` | Mouse navigation (click to focus, drag to select, scrollbars). Also starts drag-and-drop flows into editor views. |

The keyboard handler works hand-in-hand with the command payload structs in
`Commands/` so buffered key events remain deterministic.
