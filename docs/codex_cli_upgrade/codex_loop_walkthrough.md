# codex_loop_walkthrough

## objective
Use `idebridge` as Codex's authoritative bridge into IDE state for diagnostics, patch apply, and verification.

## canonical loop
1. Gather state
- `idebridge diag --json`
- Optional: `idebridge symbols --json --top_level_only` and `idebridge search --json <pattern>`

2. Draft a patch
- Codex writes unified diff to `patch.diff`.

3. Apply through IDE pipeline
- `idebridge edit --apply patch.diff`
- Optional bypass only for controlled cases: `idebridge edit --apply patch.diff --no_hash_check`

4. Verify results
- `idebridge diag --json`
- Confirm error count reduced and no regressions introduced.

5. Iterate until clean
- Repeat patch/apply/verify cycle until target diagnostics cluster is resolved.

## recommended discipline
- Keep patch scope narrow per iteration.
- Prefer one error cluster at a time.
- Use `--spill_file` when JSON is very large.
