# S7 - `core_kernel` Adoption (Final)

Status: Completed

## Goal

Adopt `core_kernel` as the loop orchestrator for scheduler + job-budget phases while preserving existing IDE input/render/wait behavior.

## Scope

- Add `mainthread_kernel` wrapper that binds:
  - `mainthread_timer_scheduler` (`CoreSched`)
  - `mainthread_jobs` (`CoreJobs`)
  - `mainthread_wake` (`CoreWake`)
- Integrate kernel lifecycle into system startup/shutdown.
- Route background orchestration through kernel tick in event loop.

## Non-Goals (this phase)

- No full rewrite of pane input/render logic into kernel modules.
- No removal of existing SDL event handling paths.

## Checklist

- [x] Add kernel wrapper module + lifecycle.
- [x] Expose core handles from timer/jobs/wake wrappers.
- [x] Integrate kernel tick into event loop background phase.
- [x] Build and verify compile.
- [x] Mark phase complete.

## Completion Note

- `core_kernel` is now integrated as the orchestration layer for scheduler + job-budget execution via `mainthread_kernel_tick(...)`.
- Existing IDE SDL event/input/render/wait flow remains in place to preserve current behavior while using kernel orchestration safely.
