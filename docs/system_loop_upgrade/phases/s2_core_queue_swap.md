# S2 - `core_queue` Message Queue Swap

Status: Completed

## Goal

Replace custom linked-list + SDL mutex queue internals with `core_queue` primitives while preserving the existing `mainthread_message_queue` API.

## Scope

- Keep message structs and public queue API unchanged.
- Back storage with `CoreQueueMutex` ring queue.
- Preserve throttled progress behavior and queue stats surface.

## Checklist

- [x] Migrate queue internals to `core_queue`.
- [x] Keep queue lifecycle and snapshot APIs stable.
- [x] Build and verify compile.
- [x] Mark phase complete.
