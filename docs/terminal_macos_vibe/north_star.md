# Terminal macOS-vibe North Star

status: `in_progress`

purpose:
- rebuild terminal architecture to deterministic, macOS-grade behavior under codex-heavy ansi output.
- remove dual interpretation of PTY bytes.

related context:
- `docs/terminal_codex_failure_dossier.md`
- legacy track (kept for reference): `docs/terminal_upgrade/`

## non-negotiable rule

there must be zero code paths where PTY bytes are interpreted outside VT parser/grid layer.

## target pipeline

`PTY -> VT parser -> ScreenBuffer(active) -> Scrollback(frozen rows) -> Renderer`

no transcript byte parsing in parallel.

## target guarantees

1. one ansi interpreter only (grid/vt state machine).
2. scrollback derived only from committed grid rows.
3. alternate buffer isolated from primary scrollback.
4. resize adds no synthetic history rows.
5. `\r` rewrites current row (`cursor_col = 0`), never appends line.
6. codex streaming/progress output stays stable (no overwrite artifacts).
7. no prompt duplication artifacts.
8. no history wipe illusion when codex starts/exits.

## implementation order (locked)

### phase 1 single-interpreter cutover
status: `completed`
doc:
- `docs/terminal_macos_vibe/phases/phase_01_single_interpreter_cutover.md`

outcome:
- disable/remove transcript parsing of PTY bytes.
- make grid state the sole source for visible rows/cursor and copy/select data.

### phase 2 scrollback commit model
status: `completed`
doc:
- `docs/terminal_macos_vibe/phases/phase_02_scrollback_commit_model.md`

outcome:
- implement frozen-row scrollback commit at top-row scroll-off.
- ensure scrollback is model data, not reconstructed text parsing.

### phase 3 alternate buffer isolation
status: `completed`
doc:
- `docs/terminal_macos_vibe/phases/phase_03_alternate_buffer_isolation.md`

outcome:
- primary + alternate buffers with 1049 enter/exit semantics.
- scrollback owned by primary only.

### phase 4 resize + deterministic reflow
status: `in_progress`
doc:
- `docs/terminal_macos_vibe/phases/phase_04_resize_deterministic_reflow.md`

outcome:
- resize updates dimensions and viewport without synthetic output.
- deterministic reflow policy from grid state only.

### phase 5 renderer parity
status: `todo`

outcome:
- renderer reads only scrollback + active buffer.
- wide-char/wcwidth correctness and utf-8 edge stability.

### phase 6 replay harness + acceptance suite
status: `todo`

outcome:
- raw PTY capture/replay path.
- deterministic assertions for codex startup/stream/resize/exit behaviors.

## acceptance scenarios (final gate)

1. pre-codex shell history remains visible after codex enter/exit.
2. codex startup box and multiline options render line-correctly.
3. codex spinner/progress rewrites same row, no fake appended rows.
4. resize during active output does not duplicate prompts.
5. alternate-screen app exit restores primary with intact scrollback.
6. unicode box drawing and wide chars render without right-edge clipping artifacts.

## out of scope until stable

- prompt tagging
- command boundary inference
- indexed terminal search
- ai overlays inside terminal rows
- session diff/export enhancements
