# Shared Core Libs Integration Plan (System Loop)

## Goal
Adopt the shared `../shared/core/` runtime libs into the IDE loop stack in a safe sequence, ending with `core_kernel` as the top-level orchestrator.

Migration order:
1. `core_time`
2. `core_queue`
3. `core_sched`
4. `core_jobs`
5. `core_workers`
6. `core_wake`
7. `core_kernel` (last)

## Current IDE Loop Baseline (already improved)
- Event-driven wait is in place (`SDL_WaitEventTimeout`) in `src/app/GlobalInfo/event_loop.c`.
- Custom wake event bridge exists (`src/core/LoopWake`).
- Custom timer scheduler exists (`src/core/LoopTimer`).
- Custom worker->main message queue exists (`src/core/LoopMessages`).

This plan replaces those custom pieces with shared libs incrementally while preserving behavior.

---

## Library Mapping Summary

### 1) `core_time` (monotonic time authority)
Use for:
- wait timeout math
- frame pacing math
- diagnostic timing snapshots

Current replacement targets:
- `SDL_GetTicks()` and parts of `SDL_GetPerformanceCounter()` arithmetic in `src/app/GlobalInfo/event_loop.c`

Integration notes:
- Keep SDL clock only where an SDL API explicitly needs milliseconds.
- Convert internal calculations to `CoreTimeNs` and compare/add via `core_time_*`.

### 2) `core_queue` (thread-safe queue primitives)
Use for:
- worker->main message queue
- optional event queue staging

Current replacement targets:
- `src/core/LoopMessages/mainthread_message_queue.*`

Integration notes:
- Start with `CoreQueueMutex` for simplicity and deterministic behavior.
- Use overflow policy explicitly per queue:
  - worker msg queue: usually `REJECT` or selective coalesce
  - progress-like queue: can use `DROP_OLDEST`

### 3) `core_sched` (deadline scheduler)
Use for:
- repeating timers: watcher poll, git poll, caret blink, debounce timers
- next deadline query for wait timeout

Current replacement targets:
- `src/core/LoopTimer/mainthread_timer_scheduler.*`

Integration notes:
- Map repeating timers to `interval_ns`.
- Use `core_sched_fire_due(..., max_fires)` to cap timer bursts.

### 4) `core_jobs` (budgeted main-thread jobs)
Use for:
- bounded execution of queued main-thread commands/jobs per loop tick

Current replacement targets:
- job/bus execution currently called inline via `tickCommandBus()` in loop path

Integration notes:
- Wrap command bus actions into `core_jobs` queue entries.
- Set `job_budget_ms` policy so input/render does not starve.

### 5) `core_workers` (fixed worker pool)
Use for:
- standardized background task dispatch and deterministic shutdown

Current replacement targets:
- ad-hoc dedicated analysis thread lifecycle in `src/core/Analysis/analysis_job.c`

Integration notes:
- Start by migrating analysis tasks only.
- Completion path should push message pointers through `core_queue` completion queue.
- Preserve existing cancel/drain semantics using `CORE_WORKERS_SHUTDOWN_*`.

### 6) `core_wake` (sleep/wake bridge)
Use for:
- main loop blocking wake path abstraction
- bridge to SDL external wake backend

Current replacement targets:
- `src/core/LoopWake/mainthread_wake.*`

Integration notes:
- Initialize `core_wake` with external backend:
  - signal_fn -> SDL push wake event
  - wait_fn -> SDL wait timeout wrapper
- Keep one wake authority to avoid split semantics.

### 7) `core_kernel` (top-level orchestrator, last)
Use for:
- deterministic phase orchestration:
  - event drain
  - worker msg drain
  - timers
  - budgeted jobs
  - update/render hint
  - idle policy

Current replacement targets:
- phased loop logic in `src/app/GlobalInfo/event_loop.c`

Integration notes:
- Register IDE modules via `CoreKernelModuleHooks`.
- Keep Vulkan/UI mutation strictly on main thread hooks.
- Switch to kernel-owned loop only after libs 1-6 are stable.

---

## Phase-by-Phase Cutover Plan

## Phase S1 - Time Unification (`core_time`)
Deliverables:
- Add thin IDE time adapter (ns<->ms conversions where needed).
- Replace internal wait/pacing arithmetic with `core_time_*`.

Completion gate:
- No behavior change; diagnostics timings still correct.

## Phase S2 - Queue Swap (`core_queue`)
Deliverables:
- Replace `mainthread_message_queue` internals with `CoreQueueMutex` (or full module swap).
- Preserve existing message schema + coalescing policy.

Completion gate:
- Worker message flow unchanged; no lost completion events.

## Phase S3 - Scheduler Swap (`core_sched`)
Deliverables:
- Replace `mainthread_timer_scheduler` with `CoreSched`.
- Keep existing timer registrations (watcher + git + future debounce).

Completion gate:
- Same timer behavior and cadence as before.

## Phase S4 - Budgeted Jobs (`core_jobs`)
Deliverables:
- Add main-thread job queue wrapper around command execution.
- Enforce per-tick job budget (`policy.job_budget_ms` equivalent).

Completion gate:
- Large queued command bursts no longer stall frame responsiveness.

## Phase S5 - Worker Pool (`core_workers`)
Deliverables:
- Move analysis dispatch to worker-pool submit.
- Route worker completions via completion queue -> main-thread drain.

Completion gate:
- Analysis lifecycle stable under repeated workspace reload/cancel.

## Phase S6 - Wake Bridge (`core_wake`)
Deliverables:
- Replace custom wake helper with `core_wake` external backend adapter to SDL.
- Main loop wait/signal path uses `core_wake`.

Completion gate:
- Wake reliability unchanged; wait path remains low CPU at idle.

## Phase S7 - Kernel Adoption (`core_kernel`, last)
Deliverables:
- Compose shared subsystems into `CoreKernel`.
- Move loop phase control from handwritten loop to kernel tick orchestration.
- Keep IDE-specific render/update in module hooks.

Completion gate:
- Behavior parity with current loop.
- Lower maintenance burden (single runtime orchestrator).

---

## Risk Notes and Guardrails
- Do not swap multiple foundational libs at once.
- Keep adaptation shims during transition (old API surface backed by new core libs).
- Preserve current diagnostics during each cutover to detect regressions quickly.
- Keep fallback path for one phase while bringing up next phase (feature flag or compile-time switch).

---

## Recommended Execution
- Implement one S-phase at a time in order.
- For each S-phase:
  - add a phase doc under `docs/system_loop_upgrade/phases/`
  - implement + build + runtime verify
  - move phase doc to `docs/system_loop_upgrade/completed/`
- `core_kernel` only after S1-S6 are stable.

