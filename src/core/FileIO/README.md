# File IO Utilities

Small cross-cutting helpers for filesystem operations that do not belong to a
specific pane.

| File | Responsibility |
| --- | --- |
| `file_ops.h/c` | Currently exposes `renameFileOnDisk` (used by the project pane). Extend this module with additional reusable file helpers as needed. |

Keep this layer light—complex file-tree logic should live in `app/GlobalInfo`
so that other modules can share it.
