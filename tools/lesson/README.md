# Source-bound C lesson step

`lesson_control.py plan --manifest lesson.json` prints a bounded shot plan.
`prepare --step ID --project-root NEW_DIR` creates an isolated before-state
workspace from the manifest's `before_text` only when its SHA-256 matches; it
refuses to replace an existing directory. `apply` takes one step through the
live IDE bridge (use `--session-file` for an external Main Edit session) and
writes a create-only
receipt after saved-source readback, a requested visible open, and optional
IDE build. Repeating a matching step returns the receipt; a changed source
or ambiguous already-after state stops for inspection.

The C division manifest is under `artifacts/ide/lesson-control-20260913/` in
the CodeWork workspace. Its before/after hashes, patch, strict C11 compile,
and expected output were verified offline. A separate packaged Main Edit app
applied the source edit, built it, and presented the bounded MCP run results
in its Run terminal. Window review showed both `integer result: 2` and
`decimal result: 2.5`. An exact-window Desktop Capture session was exported,
and a 13-second editable Capture rough cut was rendered and fully decoded.
The retained project and voiceover handoff are in the same artifact root.
Human editorial acceptance and voiceover remain separate gates.

The separate `artifacts/ide/lesson-units-20260914/` example starts with an
intentional physics-units assignment mismatch. Its task-owned Makefile uses a
pinned fisiCs compiler and explicitly enables `--overlay=physics-units`.
`build_present` displays selected lines from the digest-bound failed build;
the lesson step changes only `speed = elapsed` to `speed = distance / elapsed`.
The successful IDE build and exact-hash run then show `speed: 5.0 m/s`.
Ordinary C compilation of the portable macro source is a separate check and
does not establish units validation.
