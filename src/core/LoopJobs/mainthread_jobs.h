#ifndef MAINTHREAD_JOBS_H
#define MAINTHREAD_JOBS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "core_jobs.h"

typedef void (*MainThreadJobFn)(void* user_ctx);

typedef struct MainThreadJobsStats {
    uint32_t pending;
    uint64_t enqueued;
    uint64_t executed;
    uint64_t dropped;
    uint64_t budget_stops;
} MainThreadJobsStats;

void mainthread_jobs_init(void);
void mainthread_jobs_shutdown(void);
void mainthread_jobs_reset(void);
bool mainthread_jobs_enqueue(MainThreadJobFn fn, void* user_ctx);
size_t mainthread_jobs_run_budget_ms(uint64_t budget_ms);
size_t mainthread_jobs_run_n(size_t max_jobs);
void mainthread_jobs_snapshot(MainThreadJobsStats* out);
CoreJobs* mainthread_jobs_get_core_jobs(void);

#endif // MAINTHREAD_JOBS_H
