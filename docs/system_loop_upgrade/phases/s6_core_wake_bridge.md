# S6 - `core_wake` SDL Wake Bridge

Status: Completed

## Goal

Use `core_wake` as the wake/sleep bridge for the main loop SDL wait path and worker wake signaling.

## Scope

- Back `mainthread_wake` internals with `core_wake` external backend.
- Keep SDL user-event wake semantics for worker/main notifications.
- Route main-loop blocking wait through `mainthread_wake`.

## Checklist

- [x] Migrate `mainthread_wake` internals to `core_wake`.
- [x] Add waited-event handoff API for loop processing.
- [x] Update `event_loop` wait path to use `mainthread_wake`.
- [x] Build and verify compile.
- [x] Mark phase complete.
