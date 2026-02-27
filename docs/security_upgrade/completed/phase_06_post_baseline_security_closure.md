# Phase 06 - Post-Baseline Security Closure (`SEC-006`)

Status: `completed`
Owner: `IDE core / IPC / Build`
Depends on: `completed/phase_05_secure_config_persistence.md`

## Objective

Close the remaining security gaps discovered during the cleanup validation pass so the original hardening effort is fully carried through.

## Why This Phase Exists

Phases 1 through 5 materially improved the system and removed the highest-risk issues, but the validation pass found that two areas are still only partially complete relative to the original security intent:

- IPC `build` still uses shell execution in some non-`make` paths.
- IPC auth is materially improved, but still lacks the optional defense-in-depth measures that were part of the original target shape.

This phase captures those follow-up tasks as a focused closure pass.

## Residual Findings Being Closed

### SEC-006A - Remaining shell execution in IPC build paths

Current state:

- Default `make` builds now use a non-shell path.
- Custom configured `build_command` still constructs a shell string and runs via `popen`.
- The fallback compile path also still runs via shell.

Risk:

- The original easy IPC `profile` injection issue is fixed, but command execution still depends on shell parsing for trusted config-driven paths.
- This leaves avoidable command-execution surface in a sensitive subsystem.

Closure goal:

- Eliminate remaining shell execution where practical.
- If full removal is not practical in one pass, add an explicit interim reject policy for unsafe shell metacharacters in config-driven build commands.

### SEC-006B - Missing IPC auth defense-in-depth

Current state:

- Mutating IPC commands require a session auth token.
- Terminal-launched tooling can use the token correctly.
- However, peer credential checks and auth-failure telemetry are not implemented.

Risk:

- The main auth boundary is in place, but there is no additional same-UID verification at the socket level.
- There is also no structured visibility into repeated invalid auth attempts.

Closure goal:

- Add peer credential validation for Unix socket clients where supported.
- Add rate-limited auth failure logging/telemetry for invalid or missing mutating-command auth.

### SEC-006C - Weak fallback entropy for IPC auth token

Current state:

- Token generation uses `/dev/urandom` first.
- If that fails, it falls back to a predictable time/PID-derived generator.

Risk:

- Normal systems will almost always use the secure path, but the fallback weakens the intended security boundary if the entropy source is unavailable.

Closure goal:

- Remove the weak fallback.
- Fail closed when strong entropy is unavailable, or replace it with a stronger platform RNG source.

## Scope

In-scope:

- Complete shell-removal or shell-hardening for remaining IPC build paths.
- Add peer credential validation and rate-limited auth-failure telemetry.
- Strengthen auth token generation so it does not silently degrade.
- Update docs to reflect final security posture.

Out-of-scope:

- Full plugin out-of-process runtime implementation.
- Signed plugin trust chain / marketplace design.
- Multi-user or remote IPC trust model redesign.

## Step Plan (Execution Checklist)

1. [x] Refactor remaining `build` shell paths to argv-based execution where feasible.
2. [x] Add interim unsafe-metacharacter rejection if any shell path must temporarily remain.
3. [x] Add Unix peer credential validation on accepted IPC clients where supported.
4. [x] Add rate-limited auth-failure telemetry for mutating IPC requests.
5. [x] Replace weak auth-token fallback with fail-closed or strong platform RNG.
6. [x] Compile affected units and run a focused IPC/build smoke validation.
7. [x] Update `north_star.md` and phase docs after implementation.

## Acceptance Criteria

- No remaining IPC `build` path relies on unrestricted shell string construction without explicit validation.
- Mutating IPC requests get both token validation and peer-credential validation where the platform supports it.
- Repeated invalid auth attempts produce observable, rate-limited diagnostics.
- IPC auth token generation does not silently fall back to predictable entropy.
- Existing IDE behavior remains functionally unchanged for legitimate local use.

## Expected Outcome

After this phase, the initial security hardening program will be both:

- materially complete against the original report, and
- structurally cleaner, with fewer “partial fix” caveats left in IPC/build.

## Implementation Notes

- Updated `src/core/Ipc/ide_ipc_server.c`:
  - Replaced remaining shell-based IPC build paths with argv-based execution helpers.
  - Added non-shell fallback source-file compilation path.
  - Added Unix peer credential validation on accepted clients where supported.
  - Added rate-limited security telemetry for auth failures and peer rejections.
  - Removed weak auth-token fallback and now fail closed if strong entropy is unavailable.
- Updated validation harness:
  - `tests/idebridge_phase6_check.c`
  - `Makefile`
  - Brought the Phase 6 runtime check in line with the current authenticated IPC protocol and link dependencies.

## Validation Performed

- Compiled:
  - `build/debug/core/Ipc/ide_ipc_server.o`
- Ran:
  - `make BUILD_PROFILE=debug test-idebridge-phase6`
  - Result: passed
