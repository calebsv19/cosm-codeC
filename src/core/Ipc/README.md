# IDE IPC

This subsystem exposes the local Unix-socket control surface used by external
helpers such as `idebridge`.

| File | Responsibility |
| --- | --- |
| `ide_ipc_server.h/c` | Starts/stops the socket server, owns the session/auth token state, validates requests, and dispatches open/edit/build actions. |
| `ide_ipc_edit_apply.h/c` | Applies unified diffs to workspace files and reports per-file results back to the caller. |
| `ide_ipc_path_guard.h/c` | Resolves request paths into canonical workspace-confined paths and rejects traversal or absolute-path abuse. |

Current security posture:

- mutating commands require the per-session auth token
- supported platforms verify same-UID peer credentials on the Unix socket
- patch application is confined to existing files under the active workspace
- build execution now uses non-shell command handling rather than raw shell strings
