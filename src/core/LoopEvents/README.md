# LoopEvents

`LoopEvents` is the Phase 3 event queue core used by the IDE main loop.

Current scope in Slice 3.1:

- bounded FIFO queue for main-thread runtime events
- monotonic event sequence assignment on enqueue
- overflow rejection with explicit `events_dropped_overflow` counter
- queue counters for enqueue/process/defer/high-watermark diagnostics

Added in Slice 3.2:

- typed emitter helpers for authoritative update sites:
  - `loop_events_emit_document_edited(...)`
  - `loop_events_emit_document_revision_changed(...)`
  - `loop_events_emit_symbol_tree_updated(...)`
- `loop_events_emit_diagnostics_updated(...)`

Added in Slice 3.3:

- `loop_events_drain_bounded(...)` for deterministic per-frame event processing budgets
- deferred-backlog accounting for events left in queue after budget drain

Added in Slice 3.5:

- `event_invalidation_policy.*` for testable event -> pane invalidation routing rules
- dispatch integration coverage via `tests/loop_events_dispatch_integration_test.c`

This module intentionally does not execute handlers. It stores and returns queue
entries in deterministic order; event handling is integrated by the main-loop
dispatcher in `event_loop.c`.
