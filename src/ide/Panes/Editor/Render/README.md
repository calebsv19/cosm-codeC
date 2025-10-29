# Editor Rendering

Draws editor panes after the layout engine has positioned them.

| File | Responsibility |
| --- | --- |
| `render_editor.h/c` | Walks the editor view tree, renders gutter/background, draws text using the active font atlas, and overlays cursors, selections, and drag/drop affordances. |

Rendering is intentionally thin—text shaping and buffer access live in the
core editor files so other front ends can reuse them.
