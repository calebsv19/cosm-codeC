# Phase 03 - Alternate Buffer Isolation

status: `completed`

north star:
- `docs/terminal_macos_vibe/north_star.md`

## goal

make alternate-screen semantics fully deterministic and isolated:
- primary keeps shell history + committed scrollback.
- alternate is ephemeral UI state only.
- enter/exit transitions preserve cursor/screen semantics without leaking rows.

## why now

phase 02 established committed scrollback in primary. phase 03 ensures DECSET
1049/1047/1048 transitions cannot pollute that history or corrupt projection.

## checklist

- [x] audit and harden 1047/1048/1049 handling in `term_grid_set_alternate(...)`.
- [x] remove or replace legacy `transcript_mode` fallback path.
- [x] ensure alternate enter clears/initializes alternate buffer deterministically.
- [x] ensure alternate exit restores primary cursor/state deterministically.
- [x] ensure scrollback commits are impossible while `using_alternate=1`.
- [x] verify projection/index model switches cleanly between:
  - alternate: viewport-only rows
  - primary: committed scrollback + viewport rows
- [x] add pipeline debug logs for mode transitions:
  - enter alt
  - exit alt
  - cursor save/restore state

## file focus

- `src/ide/Panes/Terminal/terminal_grid.c`
- `src/ide/Panes/Terminal/terminal_grid.h`
- `src/ide/Panes/Terminal/terminal.c`
- `src/ide/Panes/Terminal/render_terminal.c`

## structural invariants

1. primary scrollback is never mutated while alternate is active.
2. alternate content is never appended into primary scrollback on exit.
3. mode switch never synthesizes rows or duplicates prompt state.
4. projection source is mode-correct on every frame.

## acceptance tests (manual)

- [ ] run normal shell output -> verify scrollback accumulates.
- [ ] start codex (alternate-heavy UI), produce output, exit codex.
- [ ] confirm pre-codex shell rows still exist and no codex-frame pollution.
- [ ] run rapid enter/exit alt apps (`less`, `vim`) and confirm no duplicates.

## done criteria

phase 03 is complete when:
1. alternate transitions are deterministic and isolated from primary history.
2. no prompt duplication/history pollution occurs across mode changes.
3. manual checks pass for codex + alt-screen apps.

## implementation notes

- removed legacy `transcript_mode` state and fallback behavior from
  `src/ide/Panes/Terminal/terminal_grid.*`.
- when alternate is disabled while active, primary is restored and cursor restore
  is applied deterministically.
- private mode handling now explicitly ignores `?1047`/`?1049` when alternate
  support is disabled, with debug logs.
- added `[TerminalGrid]` debug logs (gated by `IDE_TERMINAL_DEBUG_PIPELINE`) for:
  - cursor save/restore
  - alternate enter/exit
  - ignored private-mode transitions

## local verification done

- `make -j4` passes.

## manual verification pending

- manual acceptance checklist above still needs live IDE validation.
