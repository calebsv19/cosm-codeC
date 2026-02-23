# phase r3 - resize, viewport, and prompt stability

status: `in_progress`

north star:
- `docs/terminal_upgrade/north_star_reliability_rebuild.md`

## why this phase exists

current behavior still has critical correctness issues:

1. resizing can create duplicate prompts / fake empty command-looking rows.
2. codex output may flatten or overwrite rows when viewport changes.
3. prompt/input row can drift instead of staying anchored at the bottom.
4. pre-existing shell history may appear wiped when codex starts.

this phase locks down viewport invariants first, before adding more vt features.

## target behavior

1. terminal keeps one continuous history timeline (shell -> codex -> shell).
2. bottom prompt stays bottom-anchored unless user scrolls away intentionally.
3. resize changes viewport only; it does not synthesize command submissions.
4. codex/tui redraw traffic updates existing rows, not fake new shell prompts.

## implementation checklist

- [x] define a single authority for "viewport origin + cursor anchor" in terminal state.
  - done: primary-mode cursor/content rows now come from terminal grid model (`used_rows`, `cursor_row`, `cursor_col`) instead of transcript parser state.
- [x] separate "row append" from "row repaint/update" at the model API boundary.
  - done (phase slice): render/copy path now projects from emulator grid rows; transcript append noise no longer drives visible row model.
- [x] ensure resize path:
  - updates dimensions
  - remaps visible rows
  - preserves cursor/prompt anchor
  - never appends transcript entries by itself
  - done: backend resize (`SIGWINCH`) is now row/col-dimension gated, preventing repeated resize spam when only pixel size changes.
- [x] harden CR/LF handling so spinner/progress rewrites do not become extra history rows.
  - done: carriage return now consistently resets to column 0 (rewrite semantics) instead of forcing newline append behavior.
  - done: CSI cursor/erase controls are no longer suppressed in transcript-mode path, so redraw traffic can update the current row correctly.
- [x] prevent duplicate prompt stamping when shell redraws on SIGWINCH.
  - done: backend resize calls are dimension-gated and CR rewrite semantics prevent redraw prompts from being appended as fake extra rows.
- [ ] keep codex startup output from clearing previous shell transcript.
- [x] verify selection mapping after resize with updated row/col projection.
  - done (code path): selection/copy now read projected grid rows, aligned with rendered terminal source.

## instrumentation and diagnostics

- [ ] add debug counters/logs (guarded by env) for:
  - appended transcript rows
  - rewritten in-place rows
  - prompt-detect events
  - resize events and resulting viewport origin
- [ ] add one-line summary dump command for terminal model state in debug mode.

## acceptance checks (manual)

- [ ] run shell commands, resize repeatedly, confirm no duplicated prompt spam.
- [ ] start codex, allow long streaming output, resize repeatedly:
  - history keeps growing
  - no overwrite loop at pane top
  - no row flattening into single long line
- [ ] exit codex and confirm:
  - codex transcript remains scrollable
  - earlier shell commands still visible
  - prompt returns once and remains stable

## acceptance checks (deterministic/replay)

- [ ] add replay fixture for:
  - prompt -> command -> output -> prompt
  - codex startup box and streaming updates
  - resize during active streaming
- [ ] assert row count monotonicity rules:
  - append only when newline/line-commit semantics occur
  - repaint-only traffic does not increase committed row count

## files likely involved

- `src/ide/Panes/Terminal/terminal.c`
- `src/ide/Panes/Terminal/terminal_grid.c`
- `src/ide/Panes/Terminal/terminal_grid.h`
- `src/ide/Panes/Terminal/render_terminal.c`
- `src/ide/Panes/Terminal/terminal_parser.c`

## done criteria

phase r3 is complete when all checklist and acceptance items pass, with no regressions in copy/select/scroll behavior.
