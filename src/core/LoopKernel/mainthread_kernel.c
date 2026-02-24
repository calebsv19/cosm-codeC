#include "core/LoopKernel/mainthread_kernel.h"

#include <string.h>

#include "core/LoopJobs/mainthread_jobs.h"
#include "core/LoopTimer/mainthread_timer_scheduler.h"
#include "core/LoopWake/mainthread_wake.h"
#include "core_kernel.h"

static CoreKernel g_kernel;
static CoreKernelModule g_modules[1];
static bool g_initialized = false;

bool mainthread_kernel_init(void) {
    if (g_initialized) return true;

    CoreSched* sched = mainthread_timer_scheduler_get_core_sched();
    CoreJobs* jobs = mainthread_jobs_get_core_jobs();
    CoreWake* wake = mainthread_wake_get_core_wake();
    if (!sched || !jobs || !wake) return false;

    CoreKernelPolicy policy = {
        .idle_mode = CORE_KERNEL_IDLE_SPIN,
        .frame_cap_hz = 60,
        .job_budget_ms = 2,
        .max_idle_timeout_ms = 16,
        .worker_thread_count = 0,
        .coalesce_input_events = true
    };

    if (!core_kernel_init(&g_kernel, &policy, sched, jobs, wake, g_modules, 1)) {
        return false;
    }
    g_initialized = true;
    return true;
}

void mainthread_kernel_shutdown(void) {
    if (!g_initialized) return;
    core_kernel_shutdown(&g_kernel);
    memset(&g_kernel, 0, sizeof(g_kernel));
    g_initialized = false;
}

void mainthread_kernel_tick(uint64_t now_ns) {
    if (!g_initialized) return;
    core_kernel_tick(&g_kernel, now_ns);
}

void mainthread_kernel_snapshot(MainThreadKernelStats* out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->initialized = g_initialized;
    if (!g_initialized) return;
    out->render_requested = core_kernel_render_requested(&g_kernel);
    out->last_tick_work_units = core_kernel_last_tick_work_units(&g_kernel);
}
