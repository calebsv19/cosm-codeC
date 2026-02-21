# phase_04_patch_apply_pipeline

## goal
Enable Codex patch loop by supporting `idebridge edit --apply` with default hash protection, IDE-buffer-based apply semantics, and immediate diagnostics feedback.

## scope
- In scope: patch apply command path, hash checks, IDE buffer integration, response contracts.
- Out of scope: formatting/tidy tooling.

## execution checklist
1. [x] add IDE IPC command handler for `edit --apply`
2. [x] add default hash-check enforcement for patch targets
3. [x] add `--no_hash_check` path for explicit bypass
4. [x] implement unified diff parser for touched files and hunks
5. [x] implement patch application through IDE buffer objects (not direct unsafe overwrite)
6. [x] persist applied buffers through existing save pipeline
7. [x] trigger analysis refresh hooks and include diagnostics summary in response
8. [x] return structured apply result with touched files and failure reasons
9. [x] implement CLI command `idebridge edit --apply <diff_file> [--no_hash_check]`
10. [x] add runtime tests for success apply, hash mismatch failure, and malformed diff failure
11. [x] run build and targeted tests, then fix regressions
12. [x] update north star phase status and move this doc to `completed/`

## contracts

### request
- [x] includes diff payload text
- [x] includes per-file base hash values by default
- [x] includes hash-check toggle

### success result
- [x] `applied=true`
- [x] `touched_files[]`
- [x] `diagnostics_summary` with totals by severity

### failure result
- [x] structured error code/message
- [x] hash mismatch identifies offending file
- [x] malformed or unsupported patch reports deterministic reason

## test checklist
1. [x] valid patch applies and touched files are reported
2. [x] stale hash patch is rejected with deterministic error
3. [x] `--no_hash_check` allows same patch path when hashes differ
4. [x] malformed diff fails safely without partial corruption
5. [x] post-apply diagnostics summary is returned

## phase completion gate
- [x] all execution checklist items completed
- [x] all test checklist items completed
- [x] `docs/codex_cli_upgrade/north_star.md` phase_04 marked complete
- [x] this doc moved to `docs/codex_cli_upgrade/completed/`
