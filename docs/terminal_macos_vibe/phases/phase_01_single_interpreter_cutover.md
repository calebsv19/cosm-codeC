# Phase 01 - Single Interpreter Cutover

status: `completed`

north star:
- `docs/terminal_macos_vibe/north_star.md`

## goal

eliminate dual PTY byte interpretation. after this phase, only VT/grid logic may interpret terminal control stream.

## why first

current instability (history wipe illusion, prompt duplication, codex corruption) comes from competing transcript-vs-grid interpretation. no later phase is safe until this is removed.

## checklist

- [x] identify and remove/disable all PTY-byte transcript parsing entry points.
- [x] keep transcript text helpers (if needed) as derived view from grid rows only.
- [x] enforce one path in `terminal_feed_bytes`: PTY bytes -> `term_emulator_feed(...)` only.
- [x] ensure primary row count/cursor/line APIs are grid-backed only.
- [x] ensure selection/copy/readback never consult raw transcript byte parser state.
- [x] add debug assertion/log if any legacy transcript byte feed path is invoked.
- [x] keep behavior parity for existing input/send flows (no regression in typing, enter, arrows, paste).

## file focus

- `src/ide/Panes/Terminal/terminal.c`
- `src/ide/Panes/Terminal/terminal_grid.c`
- `src/ide/Panes/Terminal/render_terminal.c`
- `src/ide/Panes/Terminal/input_terminal.c`

## structural invariants to add

1. no function may parse CR/LF/CSI outside `term_emulator_feed` path.
2. row commit and cursor semantics are read from grid state.
3. renderer consumes projection APIs backed by grid state.

## instrumentation

- [x] add `IDE_TERMINAL_DEBUG_PIPELINE=1` logs:
  - bytes received
  - emulator feed call count
  - legacy transcript feed call count (must stay zero)
- [x] add one compact state snapshot log per frame in debug mode:
  - mode (primary/alternate)
  - cursor row/col
  - viewport rows/cols
  - projected rows

## acceptance tests (manual)

- [ ] run shell commands, verify history grows and remains scrollable.
- [ ] start `codex`, verify pre-codex history remains accessible.
- [ ] observe codex startup/multiline output: no flattened lines.
- [ ] exit codex: prompt returns once, no duplicate prompt spam.

## acceptance tests (deterministic)

- [ ] add replay fixture for:
  - prompt + command + output + prompt
  - codex startup block text
  - codex streaming progress updates
- [ ] verify deterministic final grid snapshot hashes under replay.

## done criteria

phase 01 is complete when:
1. transcript byte parser is no longer in active PTY interpretation path.
2. manual acceptance checks pass.
3. no regressions in terminal input/edit interactions.

## implementation notes

- removed transcript byte parser/state from `src/ide/Panes/Terminal/terminal.c`.
- removed transcript init/clear/free/session storage paths.
- enforced single feed path: `terminal_feed_bytes` -> `term_emulator_feed`.
- added debug pipeline counters + assertion for legacy feed call count.
- removed CRLF normalization in backend so PTY bytes are passed raw from
  `src/core/Terminal/terminal_backend.c`.

## local verification done

- `make -j4` passes after cutover.
