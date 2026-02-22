# Phase 6.3 Remaining Completion Plan (Unit B)

## Purpose
Document the remaining work needed to fully complete Phase 6.3 (true pane cache rebuild/reuse), including required shared Vulkan renderer changes.

## Current State
- Phase 6.3 Unit A and Unit C are complete.
- Pane invalidation and cache metadata exist in IDE.
- True cache reuse/rebuild split is deferred.
- Current rendered frame still redraws pane contents instead of compositing cached pane outputs.

## Remaining Scope (Unit B)
1. Add real per-pane render cache resources.
2. Split render path into:
   - cache rebuild path for dirty panes
   - cache reuse/composite path for clean panes
3. Preserve ordering, clipping, and overlay correctness.
4. Add robust cache lifecycle handling for resize/swapchain/theme/resource loss.
5. Validate performance gains with no visual regressions.

## Shared Vulkan Renderer Work Required
Likely required changes in `shared/vk_renderer`:
- Offscreen render target creation/destruction API for pane-sized caches.
- Begin/end rendering to offscreen pane targets.
- Composite/blit cached pane texture into main frame target.
- Invalidate/recreate hooks for swapchain recreation and size changes.
- Optional pooling/reuse strategy to avoid churn from frequent pane resizes.

## IDE Integration Work Required
In IDE (`src/engine/Render/render_pipeline.c` and pane metadata paths):
- Extend `UIPane` cache metadata to reference renderer cache handles.
- Rebuild cache only when pane dirty reasons require content refresh.
- Composite cached pane output when pane is clean.
- Keep overlay path (`rename`, drag overlays, popups) out of pane cache.
- Maintain emergency fallback: force full redraw path always available.

## Safe Rollout Strategy
1. Add renderer APIs behind a guarded feature switch.
2. Keep existing non-cached pane rendering as fallback.
3. Introduce runtime toggle for new cache-composite path.
4. Validate correctness first, then flip default.

Suggested runtime toggles:
- `IDE_PANE_CACHE_EXPERIMENTAL=1` to enable new cache path.
- `IDE_FORCE_FULL_REDRAW=1` as rollback.

## Verification Gates
- Functional:
  - resize, split, popup, rename, file watcher, terminal updates, drag overlay
  - no stale/stuck visuals after repeated interactions
- Performance:
  - `PaneRender` materially reduced in mixed interactions
  - no CPU spikes from cache thrash or excessive offscreen reallocations
- Stability:
  - swapchain recreation does not leak or orphan pane cache resources

## Risks
- Resource-lifecycle bugs during swapchain recreation.
- Cache invalidation misses causing stale panes.
- Overhead from too many offscreen targets if pooling is not controlled.

## Definition of Done
- Clean-pane reuse path is active and stable.
- Dirty-pane rebuild path is correct and minimal.
- Overlay correctness preserved.
- Measured improvement accepted with TimerHUD logs.
- Fallback path retained and documented.
