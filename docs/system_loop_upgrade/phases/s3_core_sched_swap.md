# S3 - `core_sched` Timer Scheduler Swap

Status: Completed

## Goal

Replace the custom timer scheduler internals with `core_sched` while keeping the existing `mainthread_timer_scheduler` API stable.

## Scope

- Keep existing timer scheduler API used by `event_loop`.
- Back scheduling/deadline ordering with `core_sched` heap.
- Keep one-shot/repeating behavior and snapshot stats.

## Checklist

- [x] Migrate scheduler internals to `core_sched`.
- [x] Preserve next-deadline/fire-due behavior for current loop.
- [x] Build and verify compile.
- [x] Mark phase complete.
