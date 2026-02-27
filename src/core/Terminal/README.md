# Terminal Backend

This module owns the PTY-backed process layer used by the terminal pane.

| File | Responsibility |
| --- | --- |
| `terminal_backend.h/c` | Spawns shells/processes under a PTY, resizes the PTY, reads/writes raw byte streams, tracks child exit, and stores scrollback. |

When the IDE launches terminal sessions it also injects the IDE socket path,
project root, and IPC auth token into the child environment so local helper
tools can talk back to the active session.
