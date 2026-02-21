# Phase 6.3 Plan: Pane Cache and Selective Redraw

## Goal
Stop re-rendering unchanged pane content by introducing pane-level cached draw surfaces and selective redraw composition.

## Scope
In scope:
- Add pane render cache lifecycle (create/update/reuse/invalidate).
- Re-render only dirty panes; reuse cached result for clean panes.
- Full redraw fallback on layout/theme/window scale changes.

Out of scope:
- Cross-process rendering.
- Large scene graph rewrite.

## Implementation Checklist

### Unit A: Cache data model
Files:
- `src/ide/Panes/PaneInfo/pane.h`
- `src/engine/Render/render_pipeline.c`

Tasks:
- [x] Add per-pane cache handle(s) and cache validity metadata.
- [x] Track dependencies that invalidate cache (size/theme/content scale).
- [x] Define lifecycle hooks for pane destruction.

### Unit B: Render path split
Files:
- `src/engine/Render/render_pipeline.c`

Tasks:
- [ ] Split pane draw into:
  - [ ] cache rebuild path (dirty pane)
  - [ ] cache reuse path (clean pane)
- [ ] Preserve draw order and clipping behavior.
- [ ] Keep overlay pass separate and always correct.

### Unit C: Invalidations from pane content
Files:
- editor/terminal/tool panel render + input modules

Tasks:
- [x] Mark pane dirty on content mutation (text edit, scroll, diagnostics updates).
- [x] Avoid marking unrelated panes dirty for localized changes.
- [x] Ensure pane-resize forces cache rebuild for affected pane only.

## Verification
- [ ] TimerHUD: `PaneRender` drops materially in mixed interactions.
- [ ] Resize/pathological operations still render correctly.
- [ ] No stale visual artifacts after repeated edits and panel toggles.

## Completion Gate
- Pane cache path stable for editor/tool/terminal panes.
- Full redraw fallback still available and tested.
- Ready for hardening/tuning pass in Phase 6.4.

## Deferral Note
- Unit B cache-reuse/rebuild split is deferred due to current Vulkan render-path constraints.
- Keep the existing invalidation + pane bookkeeping model as 6.3 baseline and continue optimization in 6.4/6.5.

## Current Status
- Pane cache metadata + invalidation dependency bookkeeping is wired in pane/render paths.
- Input invalidation is now pane-targeted instead of all-pane invalidation.
- Background terminal PTY updates now trigger targeted terminal-pane invalidation instead of waiting on heartbeat renders.
- Remaining 6.3 work is true cache reuse/rebuild split in render path (Unit B) for clean-pane replay.
