# Phase 06 Plan: Verification and Hardening

## Goal
Add measurable loop diagnostics and hardening safeguards so event-driven behavior can be verified and kept stable.

## Scope
In scope:
- Add runtime loop diagnostics counters.
- Add periodic diagnostic logging mode.
- Add wait-time guardrails to prevent pathological stalls.
- Finalize phase status + completion notes.

Out of scope:
- New rendering architecture.
- Threading model redesign.

## Implementation Checklist

### Unit A: Diagnostics counters
Files:
- `src/app/GlobalInfo/event_loop.c`

Tasks:
- [x] Track wait calls and blocked milliseconds.
- [x] Track wake events received (delta over time).
- [x] Track worker message queue depth peaks.
- [x] Track timer scheduler fired-count deltas.

### Unit B: Periodic reporting
Files:
- `src/app/GlobalInfo/event_loop.c`

Tasks:
- [x] Add env-gated periodic logging (1s cadence).
- [x] Log blocked vs active percentages and queue/timer stats.
- [x] Keep logging disabled by default.

### Unit C: Wait hardening
Files:
- `src/app/GlobalInfo/event_loop.c`

Tasks:
- [x] Clamp max wait timeout via optional env override.
- [x] Preserve immediate-work bypass behavior.

## Verification
- [x] Build passes (`make`).
- [ ] Diagnostic logging prints stable per-second snapshots when enabled.
- [ ] Idle loop shows high blocked ratio after startup settles.
- [ ] No interaction regressions while diagnostics disabled.

## Completion Notes
- Added loop diagnostics aggregation in `event_loop.c`:
  - per-period frames
  - wait calls
  - blocked vs active milliseconds and percentages
  - wake-event deltas
  - timer fired-count deltas
  - queue depth last/peak
- Added env-gated logging:
  - `IDE_LOOP_DIAG_LOG=1` enables 1s `[LoopDiag]` summaries.
- Added wait clamp hardening:
  - `IDE_LOOP_MAX_WAIT_MS=<n>` overrides max wait timeout.
- Compile verification passed with `make -j4`.
- Remaining checks are runtime/manual validation in live IDE session.
