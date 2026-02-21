# command_contracts_and_failures

## exit codes
- `0`: success.
- `2`: usage error (invalid CLI arguments).
- `3`: connection failure (no socket, unreachable socket).
- `4`: timeout failure (connect/read/write timeout).
- `5`: protocol failure (invalid or malformed response JSON).
- `6`: server-declared error (`ok=false` with structured error payload).

## global flags
- `--socket <path>`: explicit socket path override.
- `--timeout_ms <N>`: IPC timeout bound in milliseconds.
- `--spill_file <path>`: write raw JSON response payload to file.
- `--json`: print structured server response to stdout.

## command contracts
- `ping`
  - request args: none.
  - success fields: `proto`, `session_id`, `project_root`, `socket_path`, `server_version`, `uptime_ms`.
  - failures: connect/timeout/protocol/server errors.
- `diag [--max N]`
  - request args: optional `max`.
  - success fields: `summary`, `diagnostics[]`, `total_count`, `returned_count`, `truncated`.
- `diag-pack [--out path] [--max N]`
  - request args: optional `max` (forwarded to `diag`).
  - behavior: requests `diag`, then exports validated snapshot to a `.pack` artifact (`IDHD` header chunk + `IDJS` raw response chunk).
  - output: prints written pack path and summary counts.
- `diag-dataset [--out path] [--max N]`
  - request args: optional `max` (forwarded to `diag`).
  - behavior: requests `diag`, then exports typed `core_data` snapshot JSON (`ide_diagnostics_summary_v1`, `ide_diagnostics_rows_v1`).
  - output: prints written dataset path and summary counts.
- `symbols [--file path] [--top_level_only] [--max N]`
  - request args: optional `file`, `top_level_only`, `max`.
  - success fields: `symbols[]`, `total_count`, `returned_count`, `truncated`.
- `open path:line:col`
  - request args: `path`, `line`, `col`.
  - success fields: `path`, `line`, `col`, `applied`.
  - server failures: `open_unavailable`, `open_busy`, `open_failed`.
- `includes [--graph]`
  - request args: optional `graph`.
  - success fields: `summary`, `buckets[]`, optional `edges[]` when graph enabled.
- `search <pattern> [--regex] [--max N] [--files ...]`
  - request args: `pattern`, optional `regex`, `max`, `files[]`.
  - success fields: `match_count`, `matches[]`.
- `build [--profile debug|release]`
  - request args: optional `profile`.
  - success fields: `status`, `exit_code`, `command`, `working_dir`, `output`, `diagnostics_summary`.
- `edit --apply <diff_file> [--no_hash_check]`
  - request args: `op=apply`, `diff`, `check_hash`, `hashes`.
  - success fields: `applied`, `touched_files[]`, `diagnostics_summary`.
  - server failures: `hash_mismatch`, `edit_unavailable`, `edit_busy`, `edit_failed`.

## common server error codes
- `bad_json`, `bad_request`, `bad_proto`, `unknown_cmd`, `bad_regex`, `build_failed`, `search_failed`, `open_failed`, `edit_failed`.
