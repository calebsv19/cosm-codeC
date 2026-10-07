# codeC local MCP adapter

For interactive lesson examples, `example_run` accepts optional `stdin_text`
(UTF-8, at most 4096 bytes, no NUL). Input is supplied through a private file,
so an example that ignores stdin cannot block a pipe writer. The default is
EOF, preserving existing examples. The runtime records the supplied input and
shows it with the exact-hash completed result through `example_present`.
This is a reproducible input demonstration, not live keyboard entry.

This source-level stdio adapter exposes the existing `idebridge` session to
an agent. It requires Python 3.11+, `mcp==1.30.0`, a built `idebridge`, and
the exact active IDE project path. A Main Edit app publishes a short-lived
`agent-session.json` in its private runtime directory with mode 0600; the
adapter reads it using `--session-file`. The IDE removes it on orderly stop.
Do not copy the token into a shared or long-lived configuration file.

Example for a terminal that already belongs to the intended IDE session:

```sh
python3 tools/mcp/server.py \
  --idebridge build/targets/macOS-arm64/debug/tools/idebridge \
  --project-root /absolute/path/to/lesson-workspace
```

For an external MCP client, add `--session-file` with the absolute path to
`~/Library/Application Support/codeC Main Edit/runtime/agent-session.json`,
while using the real user home for `~`. The session file pins the IDE session
ID and workspace; if the IDE restarts, restart the MCP client. A direct IDE
terminal can continue using its inherited `MYIDE_SOCKET` and
`MYIDE_AUTH_TOKEN` without a session file.

The ten tools inspect the session, read a saved UTF-8 file, open it at a
location, apply one checked unified diff, perform checked visible typing,
query diagnostics, run and present an IDE build, or run the conventional
`build/app` example. `document_read` describes saved bytes only. `edit_apply` checks
the caller's SHA-256, binds the bridge's FNV hash to those same bytes, then
requires the IDE to check again before applying. An open unsaved buffer is
rejected. The final returned saved file is read back; callers should compare
it with their intended after-hash.

`example_run` requires the exact SHA-256 of `build/app`, takes no arguments,
uses a minimal temporary environment, and caps time and combined output. It
executes trusted lesson code locally, not in a security sandbox. After a
completed run, `example_present` takes that same executable SHA-256 and shows
the result in the IDE's Run terminal. The MCP server retains only the last
run for the current IDE session; it rejects a different hash or a restarted
IDE. The terminal view is labeled as agent-run output and truncates after
4096 safe text bytes. The IDE's `present` IPC command does no execution.
Its response confirms terminal submission, while a window review remains a
separate visual check.

`build` returns the complete bounded compiler output with its SHA-256, even
when the build fails. `build_present` accepts only the most recent build from
the same MCP process and IDE session, checked by that output hash. An optional
`focus` must occur in the actual build output; the tool shows that line and
up to two following lines in the Run terminal. The view identifies itself as
selected output and marks omitted lines. This makes a compiler diagnostic
recordable without allowing an agent to supply invented result text. A fresh
build replaces the retained result, and an IDE restart invalidates it.

The stdio contract has fake-session and bounded-run tests. The ten-tool
session, source edit, and Run-terminal presentation passed in the isolated
Main Edit bundle with live window review. A physics-units lesson also showed
the exact compiler mismatch and subsequent successful build through this
surface in the development app. Desktop Capture and Capture remain the
recording and editing controls.
`edit_type` replaces one unique caller-reviewed ASCII range. It checks the
saved SHA-256, derives the exact editor range and before/after FNV-1a bindings,
requests 16-500 ms main-loop typing, and reads the saved file back. Its receipt
reports mutation steps but does not claim captured frames; verify intermediate
visual states in the Desktop Capture recording.

Run transport-independent runtime checks with:

```sh
python3 tests/test_ide_mcp_runtime.py
```

The full stdio discovery test in `tests/test_ide_mcp.py` additionally requires
the optional Python `mcp` package.

## Workspace control (Main Edit development candidate)

`workspace_open(path)` validates an existing absolute directory, rejects unsaved
documents and active build/run tasks, queues the normal project reload, and waits
for workspace readiness. The running IDE session remains stable; workspace
generation changes. MCP requests carry the expected root and generation.
`workspace_refresh()` explicitly adopts a workspace changed through the UI after
validating the private session file and ping. A process restart still requires a
new client. Launch with `--workspace /absolute/path` to override remembered state;
`IDE_DEFAULT_WORKSPACE` remains the fallback when no explicit path is supplied.

The converter live proof is in Desktop Video Projects/C Lessons/celsius-fahrenheit/
receipts/workspace-live-proof.json. Save control is described below. Robust
recovery after a publication/reload failure, and old interactive-terminal rebinding
remain separate hardening work. The current MCP path rejects stale bindings;
legacy unbound raw protocol clients are not covered by that guarantee.

## Checked live-buffer save (development candidate)

`document_buffer_inspect(path)` reads a workspace document's live buffer, modified
state, saved-file FNV-1a hash and buffer FNV-1a hash. A closed document has no live
buffer. `document_save(path, expected_buffer_fnv64, expected_disk_fnv64)` saves
exactly that inspected buffer on the UI thread and returns saved UTF-8 readback.
A changed buffer, changed disk file, pending save queue, independent duplicate
buffers, or failed write refuses the save and preserves unsaved state. The path
must already exist inside the workspace; Save As and new untitled documents
remain unsupported. The buffer limit is 32 KiB. Successful writes use a sibling
temporary file and atomic rename, then request normal saved-file analysis.
These hashes detect conflicts; session authentication still protects requests.
Source/module/MCP tests passed on 2026-10-03. Installed live UI proof is pending.
