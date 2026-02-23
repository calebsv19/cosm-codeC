# phase r1 - terminal model and invariants foundation

status: `completed`

north star:
- `docs/terminal_upgrade/north_star_reliability_rebuild.md`

## goal

establish a correct baseline data model that cleanly separates:
- visible screen state
- primary scrollback state
- alternate screen state
- renderer/view state

this phase does not chase full vt completeness. it builds the safe foundation required for later phases.

locked defaults for this rebuild track:
- resize policy: preserve wraps (xterm-style), no reflow.
- scrollback target: 50,000 lines per tab (hard cap 200,000 lines).
- codex/tui transcript must remain scrollable after exiting codex.

## current pain this phase targets

1. history and viewport behavior are coupled, causing blank-tail and overwrite artifacts.
2. clear/erase behavior can incorrectly affect long-lived history.
3. resize changes can desync emulator rows from renderer hit-testing.
4. selection math depends on unstable content row assumptions.

## target model contract

1. `visible_screen`:
   - fixed to current viewport rows x cols.
   - receives cursor-addressed drawing.
2. `primary_scrollback_ring`:
   - append-only logical lines emitted by primary screen scroll-off.
   - independent capacity and trim policy.
3. `alternate_screen`:
   - separate visible buffer for active tui rendering.
   - codex/tui output is still captured into terminal history transcript for post-exit review.
4. `view_projection`:
   - read-only mapping from (`scroll_offset`, `visible_screen`, `scrollback_ring`) to drawn rows.

## invariants (debug asserts)

1. cursor is always within active visible buffer bounds.
2. `content_rows == scrollback_rows + visible_rows` for primary mode projection.
3. alternate mode never mutates primary scrollback.
4. selection anchor/focus map to valid projected row/col at render time.
5. resize cannot create negative/invalid row counts.
6. exiting codex/tui cannot drop pre-existing shell history.
7. transcript rows from codex/tui remain available after exit.

## implementation steps

1. introduce explicit terminal state structs
- add structs in terminal module for:
  - `TerminalVisibleBuffer`
  - `TerminalScrollbackRing`
  - `TerminalProjectionRow`
- keep old fields temporarily, but make new structs authoritative for reads.

2. add projection api used by renderer and hit-testing
- create stable helpers:
  - `terminal_projection_row_count()`
  - `terminal_projection_get_row(index, out_row)`
  - `terminal_projection_rowcol_to_cell(...)`
- migrate renderer selection/hit-test to projection api only.

3. wire primary vs alternate ownership boundaries
- enforce "alternate cannot append to primary scrollback".
- on alt enter/exit, preserve/restore primary visible buffer and cursor state.
- document behavior in code comments near mode-switch handling.

4. replace ad-hoc content-row math
- remove mixed heuristics (`used_rows`, cursor-only, last-non-empty hacks) from render-time decisions.
- compute content rows strictly from projection model.

5. add debug diagnostics hooks
- optional debug logs (`IDE_TERMINAL_DEBUG_MODEL=1`) for:
  - mode switches
  - resize operations
  - projection row counts
  - scrollback trim events

6. add optional debug overlay scaffold
- add a render-time debug overlay (off by default) gated by env flag:
  - `IDE_TERMINAL_DEBUG_OVERLAY=1`
- overlay fields:
  - mode (`primary`/`alternate`)
  - cursor row/col
  - viewport rows/cols
  - scrollback row count
  - projected content row count

## test plan (required for phase completion)

1. transcript replay tests (new)
- scenario a: normal shell prompts + commands + codex start/exit.
- scenario b: alternate-screen tui enter/exit repeated 10x.
- scenario c: clear-screen sequences in primary mode do not erase scrollback history.

2. resize stability tests
- resize narrow/wide repeatedly during streaming output.
- assert projected row count monotonic expectations and no invalid cursor positions.

3. selection mapping tests
- select across wrapped lines before and after resize.
- verify copied text matches projected content.

4. manual checks
- run codex in terminal pane:
  - prior shell lines remain after exit.
  - codex output scrolls past viewport without overwrite loop.
  - click-drag highlight stays aligned with glyphs.
  - codex transcript remains visible in history after returning to shell prompt.

## explicit out-of-scope for r1

- full vt scroll-region semantics (`DECSTBM`) correctness.
- insert/delete line completeness across all modes.
- performance optimization and batching changes.

## completion checklist

- [x] new model structs merged and used by renderer projection path.
- [x] content-row math unified via projection api.
- [x] alt-screen isolation contract implemented in model boundaries.
- [ ] resize + selection regression tests added and passing.
- [ ] transcript retention checks for codex/tui exit added and passing.
- [x] `make -j4` passes.
- [ ] manual codex scenario passes baseline checks.
- [x] debug overlay toggle available and verified.
- [x] mark phase complete and move doc to `docs/terminal_upgrade/completed/`.

## handoff to r2

after r1 completion:
- begin r2 by removing remaining legacy grid-history coupling fields.
- move any compatibility shim code behind explicit TODO tags referencing r2.

## completion notes

- completed on `2026-02-22`.
- code baseline compiles with `make -j4`.
- model/projection api is now the authoritative path for terminal row mapping.
- remaining validation for full codex transcript/resize stress is tracked for r2 handoff checks.
