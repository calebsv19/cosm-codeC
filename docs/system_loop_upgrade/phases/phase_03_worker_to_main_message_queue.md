# Phase 03 Plan: Worker -> Main Message Queue

## Goal
Add a thread-safe worker-to-main message queue and route analysis worker completion through it so UI refresh is driven by explicit worker messages.

## Scope
In scope:
- Add MPSC-style queue (mutex-protected baseline) for worker->main messages.
- Define message enum + payload ownership contract.
- Drain queue in main loop.
- Push analysis completion/cancel/error messages from worker thread.
- Main thread applies message effects (invalidate panes / refresh derived views).

Out of scope:
- Full blocking loop rewrite (`SDL_WaitEventTimeout`) in this phase.
- Full git worker migration (git is still polled on main thread for now).

## Implementation Checklist

### Unit A: Queue module
Files:
- `src/core/LoopMessages/mainthread_message_queue.h`
- `src/core/LoopMessages/mainthread_message_queue.c`

Tasks:
- [x] Add queue init/shutdown/reset.
- [x] Add thread-safe push/pop APIs.
- [x] Add optional payload free callback for owned payloads.
- [x] Add queue depth/snapshot diagnostics.

### Unit B: Message types
Files:
- `src/core/LoopMessages/mainthread_message_queue.h`

Tasks:
- [x] Add `MAINTHREAD_MSG_ANALYSIS_FINISHED` payload.
- [x] Reserve basic slots for progress/git snapshot message types.
- [x] Document ownership and free rules.

### Unit C: Producer wiring (analysis worker)
Files:
- `src/core/Analysis/analysis_job.c`

Tasks:
- [x] Push completion message on success.
- [x] Push completion message on cancellation.
- [x] Push completion message on early/failure exits.
- [x] Keep wake-event push so blocked loops can be nudged.

### Unit D: Main-loop drain + apply
Files:
- `src/app/GlobalInfo/system_control.c`
- `src/app/GlobalInfo/event_loop.c`

Tasks:
- [x] Init/shutdown message queue with system lifecycle.
- [x] Drain queue each frame loop iteration.
- [x] Apply analysis-finished message effects:
  - rebuild library flat rows
  - invalidate tool/control/editor panes as needed
- [x] Remove redundant completion edge logic that relied only on polling when safe.

### Unit E: Coalescing baseline
Files:
- `src/core/LoopMessages/mainthread_message_queue.c`

Tasks:
- [x] Add simple progress throttling helper path (time-gated enqueue).
- [x] Add last-write-wins coalescing hook for git snapshot type (placeholder-safe even if producer not yet migrated).

## Verification
- [x] Build passes (`make`).
- [ ] Analysis completion updates UI via message drain path.
- [ ] No threading regressions/crashes in analysis worker completion transitions.
- [ ] Queue depth remains stable under normal workload.

## Completion Notes
- Added queue module:
  - `src/core/LoopMessages/mainthread_message_queue.h`
  - `src/core/LoopMessages/mainthread_message_queue.c`
- Added message types and payload contracts:
  - `MAINTHREAD_MSG_ANALYSIS_FINISHED`
  - `MAINTHREAD_MSG_PROGRESS` (with throttle enqueue helper)
  - `MAINTHREAD_MSG_GIT_SNAPSHOT` (with last-write-wins coalescing hook)
- Analysis worker now pushes completion messages on success/cancel/failure paths.
- Main loop drains worker messages and applies analysis-finished UI updates.
- System lifecycle now initializes/shuts down message queue in `system_control.c`.
- Compile verification passed with `make -j4`.
- Remaining checks are runtime/manual validation in live IDE.
