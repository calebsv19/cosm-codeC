# phase 03 terminal specific text apis

goal: introduce terminal-oriented text draw/measure APIs and route terminal rendering to a dedicated monospace font path, while keeping non-terminal UI text behavior stable.

## scope

- UTF-8 + color-aware text drawing helpers for terminal rendering
- terminal text measurement helpers using explicit font parameters
- dedicated terminal monospace font accessor/path with fallback
- terminal pane integration with new APIs

## out of scope

- vulkan clip/scissor backend upgrades (phase 4)
- broad renderer performance optimization
- full terminal typography shaping engine

## ordered execution checklist

1. add terminal-oriented text APIs
- [x] `[done]` add UTF-8 + color draw helper(s) with explicit font input
- [x] `[done]` add measurement helper(s) with explicit font input for terminal use
- [x] `[done]` keep existing generic UI text helper behavior unchanged

2. add dedicated terminal monospace font path
- [x] `[done]` add `getTerminalFont()` accessor in render font module
- [x] `[done]` load terminal font from monospace candidate list with safe fallback
- [x] `[done]` preserve current active UI font behavior for non-terminal panes

3. wire terminal systems to new APIs/fonts
- [x] `[done]` switch terminal renderer text calls to terminal-specific UTF-8 color helpers
- [x] `[done]` switch terminal sizing/metrics path to terminal font accessor
- [x] `[done]` keep interaction and visual behavior coherent after font routing changes

4. validation and tests
- [x] `[done]` add compile/runtime validation coverage for new terminal text api paths
- [x] `[done]` verify build passes
- [x] `[done]` document residual gaps for phase 4 handoff

## completion gate for this phase

- [x] `[done]` terminal text rendering uses terminal-specific UTF-8/color helper path
- [x] `[done]` terminal uses dedicated monospace font accessor path (with fallback)
- [x] `[done]` non-terminal UI text path remains stable
- [x] `[done]` compile passes
- [x] `[done]` phase marked complete in `docs/terminal_upgrade/north_star.md`
- [x] `[done]` this file moved to `docs/terminal_upgrade/completed/`

## residual gaps for phase 4 handoff

- terminal text rendering still creates transient per-run text surfaces/textures; caching and batching are pending.
- vulkan clip/scissor parity is still pending, which affects strict viewport clipping behavior under Vulkan.
