#ifndef EVENT_LOOP_DIAG_HELPERS_H
#define EVENT_LOOP_DIAG_HELPERS_H

#include <stdbool.h>
#include <stdint.h>

#include "core/LoopEvents/event_queue.h"
#include "core/LoopResults/completed_results_queue.h"

void event_loop_diag_tick(uint64_t frame_start_ns, uint64_t blocked_ns, bool did_wait_call);
void event_loop_diag_note_event_emitted(IDEEventType type);
void event_loop_diag_note_event_dispatched(IDEEventType type);
void event_loop_diag_note_stale_drop_kind(CompletedResultKind kind);
int event_loop_diag_clamp_wait_timeout_ms(int timeout_ms);

#endif
