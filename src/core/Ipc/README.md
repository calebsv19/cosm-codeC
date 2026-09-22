# IDE IPC

This subsystem exposes the local Unix-socket control surface used by external
helpers such as `idebridge`.

| File | Responsibility |
| --- | --- |
| `ide_ipc_server.h/c` | Starts/stops the socket server, owns the session/auth token state, validates requests, and dispatches open/edit/build actions. |
| `ide_ipc_edit_apply.h/c` | Applies unified diffs to workspace files and reports per-file results back to the caller. |
| `ide_ipc_path_guard.h/c` | Resolves request paths into canonical workspace-confined paths and rejects traversal or absolute-path abuse. |
| `ide_ipc_session_export.h/c` | Publishes and clears a mode-0600 Main Edit session handoff for an external local agent client. |
| `ide_ipc_presentation.h/c` | Queues a bounded authenticated terminal presentation for the IDE main thread. |
| `ide_ipc_typing.h/c` | Holds one checked typing request across main-loop frames and returns its terminal result. |
| `ide_ipc_typing_edit.h/c` | Validates the saved file and selected range, inserts through editor commands at the requested cadence, saves once, and verifies readback. |

Current security posture:

- mutating commands require the per-session auth token
- supported platforms verify same-UID peer credentials on the Unix socket
- patch application is confined to existing files under the active workspace
- edit requests are policy-checked before dispatch: unified diffs have bounded
  byte, file, hunk, hunk-line, and line-length budgets; malformed non-unified
  diffs fail with `edit_policy_violation`; `check_hash=false` is accepted only
  for explicit single-file edit requests and successful responses report
  `hash_policy=unchecked_single_file`
- build execution now uses non-shell command handling rather than raw shell strings
- Main Edit uses a distinct cache/socket prefix and a private runtime session
  file; the release profile does not publish this handoff
- `present` accepts at most 4096 printable bytes, rejects terminal control
  bytes, and invokes the IDE-owned Run terminal handler on the main thread
- `type` requires the session token, existing workspace file, exact before and
  after FNV-1a hashes, exact selected text/range, bounded ASCII replacement,
  and a 16-500 ms cadence; unsaved or multiply opened buffers are rejected
- visible typing mutates at most one character per eligible main-loop frame,
  keeps rendering live between characters, saves once after the final expected
  in-memory hash, and verifies the saved file before returning success
- a timeout or session stop reports that a partial unsaved edit may remain;
  callers must inspect instead of retrying blindly

Contract lane note:

- `symbols` IPC responses include stable identity fields (`stable_id`) and, for contract `1.2.x` producers, additive ownership link identity (`parent_stable_id`).
- `diagnostics` IPC responses preserve additive taxonomy metadata for contract `1.3.x` producers:
  - `severity_id`, `category_id`, `code_id`
  - legacy fields remain present for backward compatibility consumers
- For contract `1.4.x`, optional IPC analysis lanes (for example symbols/tokens) are gated by producer-advertised contract capability flags.
- Phase 3 analysis publication also exposes `build_graph` and `memory_reports` query results from IDE-owned sidecar stores. These commands keep ingestion optional through `args.path` and publish additive context without removing legacy response fields.
