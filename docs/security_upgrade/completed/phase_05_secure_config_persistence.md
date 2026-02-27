# Phase 05 - Secure Config Persistence (`SEC-005`)

Status: `completed`
Owner: `IDE core / Preferences`
Depends on: `completed/phase_04_plugin_trust_boundary_baseline.md`

## Objective

Make local config writes private and crash-safer without changing config semantics.

## Scope

In-scope:

- Enforce config file mode `0600` on save.
- Replace in-place writes with atomic temp-file persistence.
- Validate config file ownership/permissions on load and warn on weak settings.
- Preserve existing config schema and key names.

Out-of-scope:

- Encrypting local config values.
- Moving config storage location or format.

## Step Plan (Execution Checklist)

1. [x] Add secure config file validation on load (`owner`, `mode` warnings).
2. [x] Add atomic write helper using `mkstemp` in the config directory.
3. [x] Enforce `0600` on temp/final config file writes.
4. [x] Add `fsync` before rename (and directory sync where practical).
5. [x] Compile affected unit and verify no behavioral regression in API surface.

## Acceptance Criteria

- Saved config file ends with mode `0600`.
- Save path no longer truncates the live config file before replacement.
- Weak config ownership/permissions produce warnings on load instead of failing silently.
- Existing callers (`saveWorkspacePreference`, `saveRunTargetPreference`, `saveWorkspaceBuildConfig`) behave the same.

## Implementation Notes

- Updated `src/app/GlobalInfo/workspace_prefs.c`:
  - Added load-time warnings for config owner mismatch and group/world-accessible modes.
  - Replaced in-place `fopen(..., "w")` with atomic write flow:
    - `mkstemp`
    - `fchmod(..., 0600)`
    - `fflush`
    - `fsync`
    - `rename`
  - Added config-directory `fsync` after rename for better persistence durability.
  - Kept existing config keys and caller-facing APIs unchanged.

## Validation Performed

- Compiled:
  - `build/debug/app/GlobalInfo/workspace_prefs.o`
