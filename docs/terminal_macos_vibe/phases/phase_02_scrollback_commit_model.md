# Phase 02 - Scrollback Commit Model

status: `completed`

north star:
- `docs/terminal_macos_vibe/north_star.md`

## goal

replace projected/derived scrollback behavior with explicit committed frozen rows,
owned by terminal model state and sourced only from grid scroll-off events.

## why now

phase 01 removed dual PTY interpretation. the next instability source is scrollback
being inferred from row counts/projection math rather than explicit row commits.

## checklist

- [x] introduce a committed scrollback ring storing `TermCell` rows (not text).
- [x] on primary-buffer scroll-off, copy the outgoing top row into scrollback.
- [x] ensure alternate buffer never commits to primary scrollback.
- [x] remove `projectedRows - viewportRows` synthetic scrollback derivation.
- [x] map projection indices as:
  - `0..scrollback_count-1` -> committed scrollback rows
  - remainder -> active grid viewport rows
- [x] keep selection/copy APIs working seamlessly across scrollback + viewport rows.
- [x] enforce cap/eviction policy for committed rows (drop oldest).

## file focus

- `src/ide/Panes/Terminal/terminal_grid.h`
- `src/ide/Panes/Terminal/terminal_grid.c`
- `src/ide/Panes/Terminal/terminal.c`
- `src/ide/Panes/Terminal/render_terminal.c`

## structural invariants

1. scrollback rows are model data (`TermCell` snapshots), never re-parsed text.
2. resize does not synthesize scrollback rows.
3. alternate screen does not mutate primary scrollback.
4. row readback APIs operate on the unified projection (`scrollback + viewport`).

## instrumentation

- [x] extend `IDE_TERMINAL_DEBUG_PIPELINE=1` logs with:
  - scroll-off commit count
  - scrollback ring size/cap
  - dropped-row count

## acceptance tests (manual)

- [ ] long output pushes rows into scrollback and preserves oldest-until-cap behavior.
- [ ] mouse wheel and drag selection work across committed + live rows.
- [ ] entering/exiting codex preserves pre-codex rows in primary scrollback.
- [ ] alternate-screen tools do not pollute primary scrollback.

## done criteria

phase 02 is complete when:
1. scrollback is explicit committed row storage (no synthetic derivation).
2. renderer/readback path resolves rows from committed scrollback first, then grid.
3. manual checks pass for long output + codex enter/exit + alternate screen isolation.

## implementation notes

- added committed row ring + cap/eviction in `src/ide/Panes/Terminal/terminal_grid.c`.
- committed rows are pushed only on primary-buffer scroll-off.
- projection mapping now resolves `scrollback rows + viewport rows` in
  `src/ide/Panes/Terminal/terminal.c`.
- line/string/copy helpers now read through projection cell mapping, so they
  work across committed and live rows.
- pipeline logs now include scrollback `commits`, `rows`, `cap`, and `drops`.

## local verification done

- `make -j4` passes.

## manual verification pending

- the manual acceptance checklist above still needs live IDE validation.
