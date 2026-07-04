# idebridge Tool Lane

`idebridge.c` is the local command-line bridge used by codeC/IDE tests and
automation to exercise editor-facing bridge behavior without launching the full
interactive UI.

Current ownership:

- command parsing, connection setup, request dispatch, and response handling
  remain in `idebridge.c`
- file/diff helper behavior lives in `idebridge_file_utils.*`
- non-JSON bridge error line formatting lives in `idebridge_error_format.*`
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

Verification for behavior changes should use the relevant IDE bridge test
targets from the top-level makefile. Documentation-only updates to this README
do not require rebuilding the tool.
