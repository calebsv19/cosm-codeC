# phase 01 terminal correctness

goal: make terminal emulation/parsing behavior reliable enough that codex and other modern cli output is interpreted correctly before renderer styling work.

## scope

- terminal parser state and control-sequence handling
- ansi color/style support required for expected cli output
- utf-8 decode baseline for terminal cell writing
- correctness tests and compile safety

## out of scope

- final styled rendering in pane (phase 2)
- terminal text api refactors (phase 3)
- vulkan api extension work (phase 4)

## ordered execution checklist

1. parser state persistence across backend chunk boundaries
- [x] `[done]` move escape/csi parser state from local function state to persistent `TermGrid` state
- [x] `[done]` support partial/incomplete sequence carry-over between `term_emulator_feed` calls
- [x] `[done]` verify no regressions for plain text, newline, carriage return, backspace

2. ansi sgr coverage expansion for practical cli rendering
- [x] `[done]` support bright fg/bg (90-97, 100-107)
- [x] `[done]` support 256-color palette (`38;5;n`, `48;5;n`)
- [x] `[done]` support truecolor (`38;2;r;g;b`, `48;2;r;g;b`) if straightforward during same pass
- [x] `[done]` support style reset semantics needed by tools (`0`, `22`, `24`, `39`, `49`)

3. minimal additional control sequence handling
- [x] `[done]` validate and improve erase/cursor commands already present for codex/general cli usage
- [x] `[done]` add any high-impact missing csi handling discovered during transcript testing

4. utf-8 handling baseline
- [x] `[done]` replace ascii-only write path with utf-8 decode to codepoints in terminal cells
- [x] `[done]` define and document temporary width policy for non-ascii/wide glyphs (good-enough for phase 1)
- [x] `[done]` keep unsupported edge cases explicitly marked for later phase follow-up

5. validation and tests
- [x] `[done]` add parser-focused unit tests or fixture-based checks for ansi + utf-8 cases
- [x] `[done]` add transcript fixture(s) representative of codex-like output
- [x] `[done]` verify full project compile succeeds after changes
- [x] `[done]` document any known remaining correctness gaps at phase close

## completion gate for this phase

- [x] `[done]` terminal parser handles chunked ansi sequences without corruption
- [x] `[done]` 16/256 color paths verified (and truecolor if included)
- [x] `[done]` utf-8 baseline no longer hard-drops non-ascii bytes in emulator path
- [x] `[done]` compile passes
- [x] `[done]` phase marked complete in `docs/terminal_upgrade/north_star.md`
- [x] `[done]` this file moved to `docs/terminal_upgrade/completed/`

## notes for next phase handoff

- phase 2 will consume per-cell fg/bg/attrs and render styled runs instead of flattened white text lines.
- current utf-8 width policy is temporary: codepoints are stored, but display width is treated as single-cell and final glyph shaping/width correctness will be refined in later phases.
- parser now swallows OSC payloads (BEL and ST terminators) to avoid raw escape garbage in terminal output.
