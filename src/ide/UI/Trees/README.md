# Tree Widget Helpers

Tree-style panes (project explorer, task hierarchies, etc.) share this small
rendering helper layer.

| File | Responsibility |
| --- | --- |
| `tree_renderer.h/c` | Walks `DirEntry`-like structures and renders expandable tree rows with indentation plus hover selection feedback. |
| `ui_tree_node.h/c` | Defines a simple `UITreeNode` wrapper so non-project trees can feed data into the renderer without duplicating layout code. |

Future tree panes should reuse these helpers to stay visually consistent.
