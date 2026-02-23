# phase r2 - primary/alternate scrollback separation

status: `in_progress`

north star:
- `docs/terminal_upgrade/north_star_reliability_rebuild.md`

## goals

1. ensure codex/opening interactive tools does not hide or wipe primary history.
2. decouple transcript growth from active-screen redraw behavior.
3. keep a controllable alternate-screen mode for parity testing.

## phase checklist

- [x] add runtime policy for alternate-screen enable/disable.
- [x] default policy to primary-history mode (`alt-screen disabled`).
- [x] add env override for alt-screen:
  - `IDE_TERMINAL_ENABLE_ALT_SCREEN=1`
- [ ] validate codex run:
  - old history remains visible while codex starts.
  - transcript keeps growing past pane height.
  - no single-line overwrite loop.
- [x] add dedicated transcript ring independent from grid mode.
- [ ] handoff to r3 for resize/prompt-anchor stabilization once baseline separation is verified.

## implementation notes

- implemented in grid/parser boundary:
  - `src/ide/Panes/Terminal/terminal_grid.h`
  - `src/ide/Panes/Terminal/terminal_grid.c`
- policy wiring in terminal init/session creation:
  - `src/ide/Panes/Terminal/terminal.c`
- transcript ring + transcript-projection rendering:
  - `src/ide/Panes/Terminal/terminal.c`
  - `src/ide/Panes/Terminal/render_terminal.c`

## remaining risks before closing r2

1. resize-driven redraw events still appear to generate prompt duplication in some sessions.
2. codex tui updates can still flatten into long lines when repaint-vs-append semantics blur.
3. these are now explicitly tracked as r3 scope:
   - `docs/terminal_upgrade/phases/phase_r3_resize_viewport_and_prompt_stability.md`
