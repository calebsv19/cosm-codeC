# S1 - `core_time` Unification

Status: Completed

## Goal

Move main-loop timing math to the shared monotonic time authority (`core_time`) while keeping behavior unchanged.

## Scope

- Add a local adapter in `src/core/LoopTime/` for IDE usage.
- Switch event-loop runtime timing/diag/wait elapsed calculations to `core_time`.
- Keep external behavior unchanged (same wait policy, same render cadence).

## Checklist

- [x] Add `LoopTime` wrapper (`loop_time_now_ns`, conversions).
- [x] Update `event_loop.c` timing paths to use `LoopTime`.
- [x] Build and verify compile.
- [x] Mark phase complete.

## Notes

- This phase does not change scheduler internals yet.
- Deadline/scheduler internals move in S3.
