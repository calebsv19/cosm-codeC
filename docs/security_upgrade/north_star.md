# Security Upgrade North Star

## Goal

Reduce local attack surface and enforce clear trust boundaries between IPC clients, plugins, and the IDE core process, while keeping single-user workflow smooth.

## Source Report

Primary findings tracked here come from:

- `security_best_practices_report.md` (SEC-001 through SEC-005)

## Initial Summary

Current risk is concentrated in three areas:

1. IPC mutation commands (`edit`, `build`) can be abused to write/execute outside intended workspace boundaries.
2. IPC currently trusts any same-user client process without per-command authorization.
3. Plugin loading is in-process and fully trusted, so plugin/core isolation is currently policy-only, not technical.

## Success Criteria

- `edit.apply` cannot touch files outside canonical workspace root.
- IPC build/run surfaces no longer execute untrusted text via shell interpolation.
- Mutating IPC commands require explicit session authorization.
- Plugin system has a documented trust model with at least baseline enforcement.
- Sensitive config/state files are written with strict permissions and atomic persistence.

## Risk-Based Phase Plan

### Phase 1 - Workspace Confinement for IPC Edits (SEC-001)

Status: `completed`

Objective:

- Ensure patch apply paths are workspace-confined and canonicalized.

Deliverables:

- Reject absolute target paths in `edit.apply`.
- Canonical path resolver for patch targets:
  - resolve under project root
  - validate normalized path prefix matches canonical workspace root
  - reject `..` and symlink escapes (validate real target or real parent for new files)
- Apply same path policy to hash verification paths.
- Add structured error codes for path violations.

Validation:

- Patch targeting `/tmp/x.c` fails.
- Patch targeting `../../outside` fails.
- Patch targeting in-workspace file succeeds.

### Phase 2 - Remove Shell Injection Paths (SEC-002)

Status: `completed`

Objective:

- Prevent command injection in IPC-triggered build paths.

Deliverables:

- Replace `popen(shell_cmd)` build execution path with argv-based `fork/exec` pipeline where feasible.
- Validate IPC `profile` against strict allowlist (`debug|perf`).
- Sanitize or reject unsafe metacharacters in user-configured command fields until full argv migration is complete.
- Keep output streaming behavior and diagnostics capture.

Validation:

- Malicious `profile` payload (e.g. containing `;`) is rejected.
- Standard build profile still works and returns diagnostics.

### Phase 3 - IPC Authorization and Capability Tiers (SEC-003)

Status: `completed`

Objective:

- Ensure only authorized clients can perform mutating commands.

Deliverables:

- Session secret/token handshake for mutating IPC commands (`edit`, `build`, future write ops).
- Command capability tiers:
  - read-only: `ping`, `diag`, `symbols`, `includes`, `search`
  - mutating: `edit`, `build`, etc.
- Optional peer credential checks (`getpeereid`/platform equivalent) for defense in depth.
- Add auth failure telemetry (rate-limited).

Validation:

- Client without token can run read-only commands but not mutating commands.
- Authorized client can execute mutating commands normally.

### Phase 4 - Plugin Trust Boundary Baseline (SEC-004)

Status: `completed`

Objective:

- Move plugin trust from implicit to explicit, with practical guardrails.

Deliverables:

- Document plugin security model in `src/ide/Plugin/README.md`:
  - in-process plugins are fully trusted
  - intended future out-of-process model
- Add allowlist/explicit enablement for plugin load paths.
- Add optional plugin manifest checks (name/version/capabilities) before load.
- Create roadmap doc for out-of-process plugin host.

Validation:

- Unlisted plugin path is denied.
- Listed plugin loads as before.

### Phase 5 - Secure Config Persistence (SEC-005)

Status: `completed`

Objective:

- Ensure local config writes are private and crash-safe.

Deliverables:

- Enforce config file mode `0600` on write.
- Switch to atomic write flow (`mkstemp` + `fsync` + `rename`).
- Validate config file ownership/permissions on load and log warnings for weak modes.

Validation:

- Config file permissions remain `0600` after save.
- Partial write/corruption scenarios are minimized.

### Phase 6 - Post-Baseline Security Closure (SEC-006)

Status: `completed`

Objective:

- Close the remaining partial hardening gaps found during the post-implementation validation pass.

Deliverables:

- Remove or strictly constrain remaining shell-based IPC `build` execution paths.
- Add Unix peer credential validation for IPC clients where supported.
- Add rate-limited auth-failure telemetry for mutating IPC requests.
- Remove weak fallback entropy from IPC auth token generation.

Validation:

- Remaining `build` paths no longer rely on unrestricted shell execution.
- Invalid auth attempts are visible in logs without spam.
- Auth token generation does not silently downgrade to predictable entropy.

## Suggested Execution Order

1. Phase 1 (highest data-integrity risk)
2. Phase 2 (command-exec risk)
3. Phase 3 (auth boundary)
4. Phase 5 (quick hardening and reliability)
5. Phase 4 (longer architecture effort)
6. Phase 6 (closure pass for partial fixes)

## Operating Model

- Work one phase at a time.
- Create a detailed phase doc under `docs/security_upgrade/phases/` before implementation.
- Move completed phase docs to `docs/security_upgrade/completed/`.
- Update this north-star file status after each phase.
