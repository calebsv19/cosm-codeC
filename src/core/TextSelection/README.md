# Text Selection

This module provides shared selectable-text registration for panes that render
non-editor text.

| File | Responsibility |
| --- | --- |
| `text_selection_manager.h/c` | Begins per-frame registration, stores selectable spans plus their rectangles, performs hit-testing, and exposes copy-ready span data. |

This keeps copy/select behavior consistent across panels such as diagnostics,
tool views, and other text-heavy panes.
