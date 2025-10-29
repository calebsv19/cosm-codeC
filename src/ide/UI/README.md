# UI Utilities & Layout

Shared UI infrastructure lives here. These modules keep panes lightweight by
handling layout math, resize hit-testing, and global UI state.

| File | Responsibility |
| --- | --- |
| `layout.h/c` | Calculates pane rectangles based on window size and which optional panes are visible. |
| `layout_config.h/c` | Stores configuration (default panel sizes, margins) and helper accessors used by `layout.c`. |
| `resize.h/c` | Tracks resize zones and pointer interaction so panes can be resized with the mouse. |
| `ui_state.h/c` | Global UI toggles: panel visibility, cached pointers to shared panes, theme flags, etc. |

## Subdirectories

- [`Trees/`](Trees/README.md) – Minimal tree widget framework used by the
  project tool and other hierarchical viewers.
