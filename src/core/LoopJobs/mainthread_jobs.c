#include "core/LoopJobs/mainthread_jobs.h"

#include <string.h>

#include "core_jobs.h"

enum { MAINTHREAD_JOBS_CAPACITY = 256 };

static CoreJobs g_jobs;
static CoreJob g_backing[MAINTHREAD_JOBS_CAPACITY];
static bool g_initialized = false;

void mainthread_jobs_init(void) {
    g_initialized = core_jobs_init_ex(&g_jobs,
                                      g_backing,
                                      MAINTHREAD_JOBS_CAPACITY,
                                      CORE_JOBS_OVERFLOW_REJECT);
    if (g_initialized) {
        mainthread_jobs_reset();
    }
}

void mainthread_jobs_shutdown(void) {
    g_initialized = false;
}

void mainthread_jobs_reset(void) {
    if (!g_initialized) return;
    core_jobs_init_ex(&g_jobs,
                      g_backing,
                      MAINTHREAD_JOBS_CAPACITY,
                      CORE_JOBS_OVERFLOW_REJECT);
}

bool mainthread_jobs_enqueue(MainThreadJobFn fn, void* user_ctx) {
    if (!g_initialized || !fn) return false;
    return core_jobs_enqueue(&g_jobs, fn, user_ctx);
}

size_t mainthread_jobs_run_budget_ms(uint64_t budget_ms) {
    if (!g_initialized) return 0;
    return core_jobs_run_budget(&g_jobs, budget_ms);
}

size_t mainthread_jobs_run_n(size_t max_jobs) {
    if (!g_initialized) return 0;
    return core_jobs_run_n(&g_jobs, max_jobs);
}

void mainthread_jobs_snapshot(MainThreadJobsStats* out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (!g_initialized) return;
    CoreJobsStats stats = core_jobs_stats(&g_jobs);
    out->pending = (uint32_t)core_jobs_pending(&g_jobs);
    out->enqueued = stats.enqueued;
    out->executed = stats.executed;
    out->dropped = stats.dropped;
    out->budget_stops = stats.budget_stops;
}

CoreJobs* mainthread_jobs_get_core_jobs(void) {
    if (!g_initialized) return NULL;
    return &g_jobs;
}
