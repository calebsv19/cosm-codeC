# Input Manager

SDL funnels all events through this module before anything else in the IDE
sees them. The manager normalises mouse/keyboard/device events, updates shared
state, and emits commands into the command bus.

## Top-Level Files

| File | Responsibility |
| --- | --- |
| `input_manager.h/c` | Entry point used by the frame loop. Dispatches SDL events to specialised handlers based on type. |
| `input_keyboard.h/c` | Handles global keyboard shortcuts (process exit, pane toggles). Forwards unhandled keys to the focused pane’s handler. |
| `input_mouse.h/c` | Button clicks, drags, and scroll-wheel events that apply at the window level (e.g. resize zones). |
| `input_hover.h/c` | Tracks which pane the mouse is hovering so panes can render hover feedback. |
| `input_resize.h/c` | Manages window and pane resize drag zones. |
| `input_global.h/c` | Hooks for global SDL window events (focus, quit, etc.). |
| `input_macros.h` | Convenience macro `CMD()` that queues commands on the command bus. |
| `input_command_enums.h` | Canonical list of IDE commands; shared by the entire codebase. |

## Subdirectories

- [`UserInput/`](UserInput/README.md) – Higher-level flows that need to track
  temporary state across events (e.g. rename text entry).

## Keybind Map

- See `docs/keybind_reference.md` for the current shortcut map
  across global routing and pane-specific handlers.
