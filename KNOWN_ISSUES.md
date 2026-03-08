# Known Issues (Alpha)

This release is intentionally early and feedback-driven. Expect rough edges.

## Current Limitations

- No autosave yet.
- Save your work frequently to avoid loss during crashes.

- Occasional runtime crashes can still occur.
- If possible, capture steps/repro details when reporting.

- Codex CLI terminal integration is partially functional.
- The terminal path can spawn Codex CLI/agents, but visual behavior in the IDE terminal is not fully polished yet.

- Compiler/frontend behavior is incomplete for the C behavior suite.
- Some compile/analyze results may be incorrect or unstable in edge cases.

## Practical Workarounds

- Commit often and keep short checkpoint branches.
- Use external terminal fallback when in-IDE terminal behavior is degraded.
- Restart the IDE after major workspace/config changes if behavior becomes inconsistent.

