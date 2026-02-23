# terminal upgrade north star

this document tracks the full terminal rendering/emulation upgrade to make cli tools (especially codex) visually correct inside the ide terminal pane.

## checklist tag convention

- `[todo]` not started
- `[in_progress]` active work item
- `[blocked]` work item is currently blocked
- `[done]` finished item

## phase march process

- [x] `[done]` complete the active phase checklist in order (phase 1, phase 2, phase 3, phase 4, phase 5)
- [x] `[done]` verify project compiles after phase work (phase 1, phase 2, phase 3, phase 4, phase 5)
- [x] `[done]` run/add tests when necessary for the phase changes (phase 1, phase 2, phase 3, phase 4, phase 5)
- [x] `[done]` mark completed phase checklist items with `[x]` (phase 1, phase 2, phase 3, phase 4, phase 5)
- [x] `[done]` update this north star doc to mark phase complete (phase 1, phase 2, phase 3, phase 4, phase 5)
- [x] `[done]` move completed phase file from `phases/` to `completed/` (phase 1, phase 2, phase 3, phase 4, phase 5)

## phases

### phase 1 terminal correctness first

- [x] `[done]` implement parser/emulator correctness needed for modern cli output
- [x] `[done]` add missing ansi support required for codex-level rendering quality
- [x] `[done]` ensure utf-8 handling baseline is correct enough for terminal content
- [x] `[done]` add/adjust tests for parser and transcript behavior
- [x] `[done]` compile and verify no regressions in terminal loop

reference doc:
- `docs/terminal_upgrade/completed/phase_01_terminal_correctness.md`

### phase 2 render styled terminal cells

- [x] `[done]` render per-cell background and foreground style from terminal grid
- [x] `[done]` stop flattening rows into single unstyled draw calls
- [x] `[done]` maintain selection/cursor layering with styled output
- [x] `[done]` verify codex and general cli output looks visually correct

reference doc:
- `docs/terminal_upgrade/completed/phase_02_render_styled_terminal_cells.md`

### phase 3 terminal specific text apis

- [x] `[done]` add terminal text draw/measure apis for color + utf-8 aware usage
- [x] `[done]` switch terminal to a monospace font path
- [x] `[done]` keep non-terminal ui text behavior stable
- [x] `[done]` add tests/validation for new text api paths

reference doc:
- `docs/terminal_upgrade/completed/phase_03_terminal_specific_text_apis.md`

### phase 4 vulkan backend upgrades for terminal ux

- [x] `[done]` add explicit vulkan api support needed by terminal rendering
- [x] `[done]` add compatible sdl macro mappings where appropriate
- [x] `[done]` implement clipping/scissor behavior parity for terminal viewport
- [x] `[done]` reduce obvious texture upload churn where feasible

reference doc:
- `docs/terminal_upgrade/completed/phase_04_vulkan_backend_terminal_ux.md`

### phase 5 codex focused polish pass

- [x] `[done]` validate against real codex transcripts and interaction patterns
- [x] `[done]` tune wrapping/spacing/selection and remaining visual mismatches
- [x] `[done]` finalize regression checks for general cli tools
- [x] `[done]` document final usage notes and follow-up backlog

reference doc:
- `docs/terminal_upgrade/completed/phase_05_codex_polish_pass.md`

## done phase records

- completed phase docs live in:
`docs/terminal_upgrade/completed/`

## reliability rebuild track

due to newly discovered resize/history regressions under codex/tui workloads, a new structured rebuild track is defined here:

- `docs/terminal_upgrade/north_star_reliability_rebuild.md`
