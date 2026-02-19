# phase 04 vulkan backend terminal ux

goal: add Vulkan-side capabilities needed for terminal viewport correctness and SDL compatibility parity, especially clipping/scissor behavior required by the terminal pane.

## scope

- explicit Vulkan clip/scissor API for renderer state
- SDL compatibility macros for clip functions
- IDE clipping path wired to work under Vulkan backend
- low-effort draw overhead reductions where feasible in this pass

## out of scope

- full text/glyph atlas cache system
- deep renderer architecture changes

## ordered execution checklist

1. vulkan clip/scissor API
- [x] `[done]` add clip state to Vulkan draw state
- [x] `[done]` add `set/get/is_enabled` clip APIs to shared Vulkan renderer
- [x] `[done]` apply active scissor on Vulkan draw calls

2. SDL compat macro parity
- [x] `[done]` map SDL clip APIs to Vulkan clip APIs in compatibility header
- [x] `[done]` extend macro smoke check to include clip APIs

3. IDE integration under Vulkan
- [x] `[done]` enable `pushClipRect/popClipRect` path to call renderer clip APIs under Vulkan
- [x] `[done]` verify terminal viewport clipping behavior works under Vulkan backend

4. overhead reduction (feasible pass)
- [x] `[done]` keep existing run-based terminal rendering path to avoid per-cell text draws
- [x] `[done]` ensure no regressions in build/runtime validation

## completion gate for this phase

- [x] `[done]` Vulkan renderer exposes clip API and uses scissor consistently
- [x] `[done]` SDL compatibility layer includes clip-related mappings
- [x] `[done]` IDE clip stack path works with Vulkan backend
- [x] `[done]` build passes
- [x] `[done]` phase marked complete in `docs/terminal_upgrade/north_star.md`
- [x] `[done]` this file moved to `docs/terminal_upgrade/completed/`

## residual gaps for phase 5 handoff

- terminal text path still relies on transient SDL surface-to-texture uploads per run; quality is correct but performance can improve with caching.
- strict visual parity checks against long real codex transcripts remain as final polish validation.
