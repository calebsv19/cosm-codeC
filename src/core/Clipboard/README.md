# Clipboard

This module is a small wrapper around the operating system clipboard.

| File | Responsibility |
| --- | --- |
| `clipboard.h/c` | Copies UTF-8 text to the system clipboard, reads it back, and keeps a local cached copy so paste still works when the platform clipboard is unavailable. |

UI code should use this helper instead of talking to SDL clipboard APIs
directly so fallback behavior stays consistent across panes.
