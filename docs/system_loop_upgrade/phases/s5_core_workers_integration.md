# S5 - `core_workers` Background Worker Integration

Status: Completed

## Goal

Adopt `core_workers` into the IDE runtime stack in a safe baseline form without regressing analysis thread stack requirements.

## Scope

- Wire `core_workers` into build/includes and runtime stack availability.
- Keep analysis execution on explicit 8MB-stack SDL worker thread (safety).
- Leave analysis scheduler/queue/wake behavior unchanged.

## Checklist

- [x] Add worker-pool availability in the runtime build stack.
- [x] Preserve explicit-stack analysis worker path for stability.
- [x] Keep existing completion message/wake semantics.
- [x] Build and verify compile.
- [x] Mark phase complete.

## Defer Note

- Full migration of analysis execution to `core_workers` is deferred until configurable worker stack sizing is available in shared lib or analysis recursion depth is reduced.
