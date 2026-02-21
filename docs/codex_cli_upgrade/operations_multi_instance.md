# operations_multi_instance

## default session routing
- IDE sets `MYIDE_SOCKET` and `MYIDE_PROJECT_ROOT` in the PTY it launches.
- `idebridge` uses `MYIDE_SOCKET` by default, so commands route to the same IDE instance that owns that PTY.

## multiple IDE windows
- Each IDE window/session exposes a unique Unix socket path.
- PTYs launched by each window receive that window's socket env var.
- Running `idebridge` inside a given PTY naturally targets the matching IDE session.

## explicit override
- Use `idebridge --socket <path> ...` to target a non-default session.
- This is the canonical way to inspect or control another IDE instance from a different shell.

## operator playbook
1. In each terminal, run `echo $MYIDE_SOCKET` to verify bound session.
2. Run `idebridge ping --json` to confirm session metadata.
3. If needed, reroute with `--socket` and re-run `ping`.

## safety notes
- Socket permissions are restrictive (`0600`) to limit cross-user access.
- Keep request payloads machine-readable and deterministic when scripting.
