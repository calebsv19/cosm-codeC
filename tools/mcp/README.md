# codeC local MCP adapter

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
