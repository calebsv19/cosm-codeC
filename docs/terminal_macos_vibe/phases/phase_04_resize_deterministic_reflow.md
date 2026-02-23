# Phase 04 - Resize + Deterministic Reflow

status: `in_progress`

north star:
- `docs/terminal_macos_vibe/north_star.md`

## goal

make terminal resize behavior deterministic and artifact-free:
- no synthetic output/history rows
- no prompt duplication from resize redraws
- stable cursor anchoring
- predictable content presentation across width/height changes

## why now

phase 01-03 established single interpreter, committed scrollback, and alt
isolation. resize is now the main source of visual instability and history
artifacts, especially with codex-heavy redraw output.

## checklist

- [x] enforce resize contract:
  - call `TIOCSWINSZ`
  - child receives `SIGWINCH`
  - no direct text/history synthesis in IDE layer
- [x] remove remaining assumptions that viewport changes imply content growth.
- [x] define and implement deterministic width policy:
  - initial pass: preserve existing cell rows, crop/extend per row by width
  - no synthetic wraps committed to scrollback during resize
- [x] ensure height change only affects viewport windowing, not logical history.
- [x] harden cursor clamping after resize for both primary/alternate buffers.
- [ ] ensure scroll offsets are clamped without forcing fake prompt rows.
- [x] add debug logs (pipeline mode) for resize snapshots:
  - old/new viewport
  - old/new grid dimensions
  - scrollback rows before/after
  - cursor before/after

## file focus

- `src/ide/Panes/Terminal/terminal.c`
- `src/ide/Panes/Terminal/terminal_grid.c`
- `src/core/Terminal/terminal_backend.c`
- `src/ide/Panes/Terminal/render_terminal.c`

## structural invariants

1. resize never appends to scrollback directly.
2. resize never replays/interprets PTY bytes.
3. resize cannot duplicate prompts by local synthesis.
4. alternate mode resize remains isolated from primary scrollback.

## acceptance tests (manual)

- [ ] run shell prompt + commands, resize aggressively (both directions):
  - no duplicate prompt spam
  - no phantom blank-row growth
- [ ] run codex, resize during active output:
  - history remains scrollable
  - no line-overwrite collapse/jitter loops
- [ ] exit codex after resize stress:
  - pre-codex and codex history remain intact

## done criteria

phase 04 is complete when:
1. resize no longer generates synthetic history artifacts.
2. cursor/prompt behavior remains stable under aggressive resize.
3. manual codex + shell resize checks pass.

## implementation notes (in progress)

- `term_grid_resize(...)` now preserves tail-visible content deterministically for
  primary and alternate buffers, with per-row crop/extend by new width.
- scrollback rows are preserved across width resize (no forced ring clear).
- resize path asserts no synthetic scrollback mutation:
  - scrollback row count unchanged
  - scrollback commit count unchanged
- resize pipeline logs now include old/new viewport, grid, cursor, and
  scrollback counters.

## local verification done

- `make -j4` passes after Phase 4 updates so far.
