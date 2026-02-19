# phase 02 render styled terminal cells

goal: render terminal grid cells using their stored style data (`fg`, `bg`, `attrs`) instead of flattening each row into unstyled white text.

## scope

- per-cell background rendering from `TermCell.bg`
- styled foreground text rendering from `TermCell.fg` and `attrs`
- preserve selection and cursor layering with styled output
- keep scroll/viewport behavior stable

## out of scope

- terminal text api refactor (phase 3)
- vulkan clipping/scissor api upgrades (phase 4)
- deeper performance optimization passes

## ordered execution checklist

1. replace flattened line rendering path
- [x] `[done]` remove row flattening that discards per-cell style in terminal pane renderer
- [x] `[done]` draw terminal rows from grid cells with explicit x/y cell positioning

2. draw per-cell background color
- [x] `[done]` render background color runs from `TermCell.bg` across visible rows
- [x] `[done]` keep behavior deterministic for default and non-default background colors

3. draw styled foreground text
- [x] `[done]` render text using per-run/per-cell foreground color from `TermCell.fg`
- [x] `[done]` support utf-8/codepoint output path from grid cells in renderer
- [x] `[done]` apply basic attr rendering needed now (underline/bold approximation where practical)

4. preserve overlays and interaction visuals
- [x] `[done]` keep selection highlight visually coherent with styled text
- [x] `[done]` keep cursor positioning coherent with grid cell coordinates

5. validation
- [x] `[done]` build and verify terminal pane compiles and links cleanly
- [x] `[done]` manually verify codex-like cli output is visibly improved
- [x] `[done]` document residual visual gaps for phase 3/4 follow-up

## completion gate for this phase

- [x] `[done]` no flattened white-only terminal row rendering path remains
- [x] `[done]` per-cell/runs bg + fg colors are rendered in terminal pane
- [x] `[done]` selection/cursor layering remains usable
- [x] `[done]` compile passes
- [x] `[done]` phase marked complete in `docs/terminal_upgrade/north_star.md`
- [x] `[done]` this file moved to `docs/terminal_upgrade/completed/`

## residual gaps for phase 3/4

- text rendering currently creates transient text surfaces/textures per run; performance optimization is deferred.
- terminal clipping on vulkan backend still depends on later scissor/clip work (phase 4).
- monospace font switch + terminal-specific text APIs are deferred to phase 3.
