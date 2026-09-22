# idebridge Tool Lane

`idebridge.c` is the local command-line bridge used by codeC/IDE tests and
automation to exercise editor-facing bridge behavior without launching the full
interactive UI.

Current ownership:

- command parsing, connection setup, request dispatch, and response handling
  remain in `idebridge.c`
- file/diff helper behavior lives in `idebridge_file_utils.*`
- non-JSON bridge error line formatting lives in `idebridge_error_format.*`
- checked visible-typing argument validation and JSON construction live in
  `idebridge_type.*`
- the tool remains source-adjacent to packaging tools under `ide/tools/`
- extraction work is tracked separately in the private IDE large-file
  decomposition plan and should not be mixed with docs-only structure passes

Structure notes:

- this lane currently has the main tool file `idebridge.c` plus adjacent
  tool-local helpers
- `idebridge.c` is below the general large-file warning threshold after the
  file/diff helper and error-format helper extractions
- future changes should prefer extracting separable command, transport,
  response-formatting, or test-helper code into adjacent files before adding
  new behavior inline

Error formatting:

- non-JSON transport, response, and server failures use one stable line shape:
  `idebridge: stage=<stage> code=<code> message="<message>" detail="<detail>"`
- `detail` is omitted when no bounded detail is available
- `--json` server-error output still preserves the raw JSON response and
  existing exit-code behavior

Agent edit guard:

- `edit --apply` accepts `--expected_fnv64 <16 lowercase hex digits>` for a
  single target. It refuses the edit if current source differs from that
  caller-reviewed snapshot. The IDE independently checks the same hash again
  on its main thread immediately before applying the diff.
- The local MCP adapter in `tools/mcp/` supplies this guard after checking a
  caller-provided SHA-256. It reads back saved source after an edit. This is
  source proof, not evidence of what a viewer saw in a GUI or recording.

Visible typing:

- `type` carries one reviewed file range, exact before/after FNV-1a hashes,
  expected selected text, replacement text, and cadence to the authenticated
  IDE session.
- The IDE performs the actual insertion over multiple rendered frames and
  saves once. The `edit_type` MCP tool adds caller-facing SHA-256 checks,
  unique-range selection, and saved-file readback.
- A successful typing receipt reports available mutation steps. It deliberately
  leaves `intermediate_frames_captured=false`; Desktop Capture evidence must
  prove that separate visual layer.

Verification for behavior changes should use the relevant IDE bridge test
targets from the top-level makefile. Documentation-only updates to this README
do not require rebuilding the tool.
