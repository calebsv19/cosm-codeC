# IDE Current Architecture Status

Last updated: 2026-03-26
Audience: public/stable readers

## Summary

The IDE runtime is currently operating on an event-driven main-thread apply model for core analysis surfaces, with deterministic worker-result handoff and stale-result dropping.

This means:

1. worker jobs compute and publish results
2. main thread is the single apply authority for runtime state transitions
3. stale analysis results are dropped before visible apply
4. pane updates are routed through event-driven invalidation instead of unconditional redraw loops

## Landed Foundations

1. document revision tracking + stale guards for result apply
2. coalesced scheduler keys (latest-wins) for burst-edit workloads
3. edit transaction/debounce boundaries for heavy analysis scheduling
4. bounded runtime event queue with deterministic per-frame dispatch
5. completed-results queue for worker -> main-thread handoff

## Current Phase State

1. symbols lane migration: complete
2. diagnostics lane migration: complete
3. analysis/index residual lane migration: complete
4. Phase 4 hardening/guardrails and closure gate: complete
5. Phase 5 carry-forward hardening/observability closure: complete

## Validation Baseline

Current baseline command lanes:

1. `make -C ide test-fast`
2. `make -C ide test-phase3`
3. `make -C ide test-phase4`
4. `make -C ide test-phase5`

These include event queue/dispatch tests, scheduler coalescing + index-key policy checks, mutation-guard helper regressions, loop diagnostics config parsing regressions, idle-efficiency sanity coverage, idebridge runtime checks, and targeted regressions for bridge/startup diagnostics+symbols event emission behavior.

Latest closure check: full `make -C ide test-phase5` reran green during Phase 5.4 closure on 2026-03-26.
