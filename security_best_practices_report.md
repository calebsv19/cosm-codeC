# Security Best Practices Report

## Executive Summary

This IDE is a local, single-user system, but there are high-impact trust-boundary gaps between IPC clients, plugins, and the core process. The most important issue is that IPC-driven edit apply can target absolute paths outside the workspace, enabling arbitrary file overwrite as the IDE user. The next highest issue is shell command construction from unsanitized IPC/config values, enabling command injection.

## Scope and Method

- Reviewed C sources in `src/core/Ipc`, `src/core/BuildSystem`, `src/core/Terminal`, `src/ide/Plugin`, and workspace preference handling.
- Focused on plugin/core boundary, IPC trust model, and command/file write surfaces.
- Note: the `security-best-practices` skill is framework-focused for Python/JS/Go; this assessment is a C-specific best-effort review.

## Critical Findings

### SEC-001: IPC edit apply allows writes outside workspace via absolute paths
- Severity: Critical
- Impact: A client with socket access can overwrite arbitrary files writable by the IDE user, not just workspace files.
- Evidence:
  - Absolute patch targets are accepted directly: `if (fp->new_path[0] == '/') ...` in `ide_ipc_apply_unified_diff` [src/core/Ipc/ide_ipc_edit_apply.c:553](/Users/calebsv/Desktop/CodeWork/ide/src/core/Ipc/ide_ipc_edit_apply.c:553)
  - Relative paths are joined without canonical workspace-bound checks: [src/core/Ipc/ide_ipc_edit_apply.c:557](/Users/calebsv/Desktop/CodeWork/ide/src/core/Ipc/ide_ipc_edit_apply.c:557)
  - Hash verification path logic also permits absolute targets: [src/core/Ipc/ide_ipc_server.c:965](/Users/calebsv/Desktop/CodeWork/ide/src/core/Ipc/ide_ipc_server.c:965)
- Recommendation:
  - Reject absolute paths for `edit.apply`.
  - Canonicalize candidate path (`realpath`) and enforce prefix under canonical workspace root.
  - Reject `..` escapes and symlink escapes by validating resolved target parent path.

### SEC-002: IPC build command is shell-injectable via untrusted arguments
- Severity: Critical
- Impact: An IPC client can inject arbitrary shell syntax into the build command and execute arbitrary commands as the IDE user.
- Evidence:
  - Profile value is embedded directly into shell command: [src/core/Ipc/ide_ipc_server.c:838](/Users/calebsv/Desktop/CodeWork/ide/src/core/Ipc/ide_ipc_server.c:838)
  - Full command is executed with `popen(shell_cmd, "r")`: [src/core/Ipc/ide_ipc_server.c:859](/Users/calebsv/Desktop/CodeWork/ide/src/core/Ipc/ide_ipc_server.c:859)
- Recommendation:
  - Replace shell command strings with `fork/execve` argument vectors where possible.
  - Strictly validate `profile` against allowlist (e.g. `debug|perf`) before use.
  - If shell execution must remain, apply robust shell escaping and deny metacharacters.

## High Findings

### SEC-003: No strong IPC client authentication beyond filesystem permissions
- Severity: High
- Impact: Any process running as the same user can issue powerful IPC commands (`build`, `edit`, `open`), which breaks intended boundaries with plugins/tools.
- Evidence:
  - Server accepts connections and requests without peer credential validation: [src/core/Ipc/ide_ipc_server.c:1423](/Users/calebsv/Desktop/CodeWork/ide/src/core/Ipc/ide_ipc_server.c:1423)
  - Socket is mode `0600` (good baseline) but no per-client auth token/capability layer: [src/core/Ipc/ide_ipc_server.c:1495](/Users/calebsv/Desktop/CodeWork/ide/src/core/Ipc/ide_ipc_server.c:1495)
- Recommendation:
  - Add command authorization tiers (read-only vs mutating commands).
  - Use a session secret token handshake for mutating commands.
  - Optionally check peer credentials (`SO_PEERCRED` / `getpeereid`) and enforce same UID + optional parent PID/session constraints.

## Medium Findings

### SEC-004: Plugins are fully trusted in-process with no isolation boundary
- Severity: Medium
- Impact: A compromised or buggy plugin has full process privileges (memory, filesystem, IPC state), so plugin/core boundary is effectively nonexistent.
- Evidence:
  - Plugins loaded via `dlopen` into IDE process: [src/ide/Plugin/plugin_interface.c:29](/Users/calebsv/Desktop/CodeWork/ide/src/ide/Plugin/plugin_interface.c:29)
  - No signature/manifest checks or permission model in plugin interface: [src/ide/Plugin/plugin_interface.h:14](/Users/calebsv/Desktop/CodeWork/ide/src/ide/Plugin/plugin_interface.h:14)
- Recommendation:
  - For real boundary, move plugins out-of-process and communicate over constrained IPC API.
  - If staying in-process, require plugin allowlist + signed manifests + explicit capability declarations.

### SEC-005: Config file write path does not explicitly enforce file mode
- Severity: Medium
- Impact: Depending on user umask, config may become more readable than intended, exposing workspace/run/build command details.
- Evidence:
  - Config directory is created with `0700` (good): [src/app/GlobalInfo/workspace_prefs.c:42](/Users/calebsv/Desktop/CodeWork/ide/src/app/GlobalInfo/workspace_prefs.c:42)
  - Config file is written via `fopen(..., "w")` without explicit chmod/fchmod: [src/app/GlobalInfo/workspace_prefs.c:139](/Users/calebsv/Desktop/CodeWork/ide/src/app/GlobalInfo/workspace_prefs.c:139)
- Recommendation:
  - Enforce `0600` on config file after open/create.
  - Consider atomic write (`mkstemp` + `rename`) to avoid partial writes.

## Suggested Hardening Order

1. Fix `SEC-001` workspace path confinement for edit apply.
2. Fix `SEC-002` shell injection in IPC build.
3. Add IPC authorization/token model (`SEC-003`).
4. Define plugin trust model and isolation roadmap (`SEC-004`).
5. Tighten config file mode and atomic write (`SEC-005`).

