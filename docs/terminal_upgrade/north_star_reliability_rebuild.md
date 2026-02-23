# terminal reliability rebuild north star

goal: stabilize terminal behavior under resize, codex/tui apps, selection, and scrollback by moving to a strict model-first implementation with regression gates between phases.

this plan intentionally replaces ad-hoc terminal fixes with a clean execution track.

## success criteria

- terminal history persists correctly before, during, and after codex sessions.
- resize never corrupts content, selection coordinates, or cursor placement.
- primary scrollback and alternate screen are isolated and predictable.
- selection/copy maps to visible glyph positions after any resize.
- cli parity is close to mac terminal for interactive workflows.

## non-goals (for this track)

- full xterm feature completeness.
- shell-level features outside pty/vt emulation (prompt theming/plugins).
- performance micro-optimizations before correctness baseline is locked.

## principles

1. single source of truth for terminal state.
2. explicit invariants checked at runtime in debug builds.
3. no mixed responsibilities between emulator, history store, and renderer.
4. every phase ships with deterministic transcript tests.
5. no behavior changes without a matching acceptance case.

## architecture target

1. pty stream input layer:
   - raw byte ingestion and chunk boundaries only.
   - no rendering assumptions.
2. vt emulator core:
   - state machine for c0/esc/csi/osc/dcs (scoped to supported set).
   - primary + alternate screen with explicit switch semantics.
   - cursor/save-restore/scroll-region behavior bounded by viewport.
3. screen + scrollback model:
   - screen buffer represents visible terminal rows only.
   - scrollback is a separate append-only line ring for primary screen output.
   - codex/tui sessions must preserve transcript history after exit.
   - behavior target: entering/exiting codex never wipes prior shell history.
4. renderer + interaction:
   - purely a view over model state.
   - selection and hit-testing map through a stable row/col projection API.
5. controller:
   - resize policy updates viewport first, then emulator layout, then renderer metrics.

## phase map

### phase r1: model and invariants foundation

status: `completed`

doc:
- `docs/terminal_upgrade/completed/phase_r1_terminal_model_and_invariants.md`

### phase r2: primary/alternate + scrollback separation

status: `in_progress`

doc:
- `docs/terminal_upgrade/phases/phase_r2_primary_alternate_scrollback_separation.md`

outcome:
- move from giant single grid to `visible_screen + scrollback_ring`.
- make alt-screen swap lossless and isolated while preserving codex transcript.
- eliminate duplicate startup runs and ensure one analysis/render pass owns terminal updates.

### phase r3: resize correctness and reflow policy

status: `in_progress`

doc:
- `docs/terminal_upgrade/phases/phase_r3_resize_viewport_and_prompt_stability.md`

outcome:
- define hard resize semantics:
  - preserve-wrap mode (xterm-like stable behavior, no reflow).
  - optional future reflow mode behind flag (not part of this phase).
- guarantee selection/cursor mapping survives resize.
- guarantee prompt/input row remains bottom-anchored after resize.
- guarantee resize never injects synthetic empty commands or duplicate prompt rows.

### phase r4: tui control semantics pass

status: `todo`

outcome:
- implement and validate core csi semantics needed by codex/tui:
  - scroll regions (`DECSTBM`)
  - insert/delete lines
  - erase variants with correct region rules
  - save/restore cursor variants
  - cursor horizontal/vertical positioning parity for codex menus and status lines
  - bracketed-paste behavior isolation from normal typed input

### phase r5: interaction correctness pass

status: `todo`

outcome:
- selection hit-testing parity across all scroll offsets.
- copy behavior from wrapped and clipped rows.
- cursor visuals tied to emulator cursor only.
- terminal mouse/keyboard interactions do not corrupt history rows.
- codex multi-line option lists render on distinct rows (no flattening into one long row).

### phase r6: hardening and regression harness

status: `todo`

outcome:
- transcript replay tests for codex-like sessions.
- resize stress tests.
- selection mapping tests.
- acceptance scripts for "mac terminal parity baseline".
- add deterministic fixtures for:
  - codex startup box
  - codex spinner/status updates
  - codex rate-limit model selection list
  - codex exit back to shell prompt with retained prior history

## acceptance scenarios (must pass before marking rebuild complete)

1. pre-codex shell history remains visible after entering/exiting codex.
2. codex output can exceed viewport height and remains scrollable.
3. repeated resize during codex run does not corrupt rows or selection.
4. switching between normal shell and full-screen tui does not wipe primary history.
5. click-drag selection remains pixel-aligned to shown glyphs after resize.
6. codex exit returns to normal shell prompt while full codex session remains scrollable.
7. codex startup does not erase pre-existing shell history lines.
8. resizing during codex run does not emit duplicate prompts or fake empty command entries.
9. codex multi-line menus/options render as separate lines and remain selectable via keyboard/mouse.

## locked decisions

1. resize policy: preserve wraps (xterm-style), no reflow.
2. codex/tui history policy: keep full transcript after exit.
3. scrollback defaults:
   - default soft target: 50,000 lines per terminal tab
   - hard cap safety ceiling: 200,000 lines per tab
4. parity target: mac terminal-like behavior for history, resize stability, prompt position, and tui transitions.
5. debug tooling: include an optional terminal debug overlay for projection/cursor/mode state.

## remaining spec questions

1. do we need bracketed-paste visual indicators or only protocol support? (default: protocol only)
2. minimum supported vt feature set for "good codex parity" in this ide? (track in r4 fixtures)

## workflow rules for this rebuild

1. do not merge terminal behavior changes outside active rebuild phase docs.
2. each phase starts with a dedicated plan doc in `docs/terminal_upgrade/phases/`.
3. only move a phase doc to `completed/` after transcript + manual acceptance checks pass.
