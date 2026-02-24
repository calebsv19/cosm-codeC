#include "core/LoopTimer/mainthread_timer_scheduler.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "core/LoopTime/loop_time.h"
#include "core_sched.h"
#include "core_time.h"

#define MAINTHREAD_TIMER_MAX 64

typedef struct MainThreadTimerEntry {
    int id;
    bool in_use;
    bool repeating;
    uint64_t interval_ns;
    CoreSchedTimerId sched_id;
    MainThreadTimerCallback cb;
    void* user_data;
    char label[32];
} MainThreadTimerEntry;

static CoreSched g_sched;
static CoreSchedTimer g_sched_backing[MAINTHREAD_TIMER_MAX];
static MainThreadTimerEntry g_timers[MAINTHREAD_TIMER_MAX];
static int g_next_timer_id = 1;
static uint32_t g_fired_count = 0;
static bool g_scheduler_initialized = false;

static MainThreadTimerEntry* find_timer_by_id(int timer_id) {
    if (timer_id <= 0) return NULL;
    for (int i = 0; i < MAINTHREAD_TIMER_MAX; ++i) {
        if (g_timers[i].in_use && g_timers[i].id == timer_id) {
            return &g_timers[i];
        }
    }
    return NULL;
}

static MainThreadTimerEntry* find_timer_by_sched_id(CoreSchedTimerId sched_id) {
    if (sched_id == 0) return NULL;
    for (int i = 0; i < MAINTHREAD_TIMER_MAX; ++i) {
        if (g_timers[i].in_use && g_timers[i].sched_id == sched_id) {
            return &g_timers[i];
        }
    }
    return NULL;
}

static int alloc_timer_slot(void) {
    for (int i = 0; i < MAINTHREAD_TIMER_MAX; ++i) {
        if (!g_timers[i].in_use) return i;
    }
    return -1;
}

static void sched_fire_cb(CoreSchedTimerId id, void* user_ctx) {
    (void)user_ctx;
    MainThreadTimerEntry* e = find_timer_by_sched_id(id);
    if (!e || !e->in_use || !e->cb) return;
    MainThreadTimerCallback cb = e->cb;
    void* user_data = e->user_data;
    if (!e->repeating) {
        memset(e, 0, sizeof(*e));
    }
    cb(user_data);
    g_fired_count++;
}

void mainthread_timer_scheduler_init(void) {
    mainthread_timer_scheduler_reset();
}

void mainthread_timer_scheduler_shutdown(void) {
    mainthread_timer_scheduler_reset();
}

void mainthread_timer_scheduler_reset(void) {
    memset(g_timers, 0, sizeof(g_timers));
    memset(g_sched_backing, 0, sizeof(g_sched_backing));
    g_next_timer_id = 1;
    g_fired_count = 0;
    g_scheduler_initialized = core_sched_init(&g_sched, g_sched_backing, MAINTHREAD_TIMER_MAX);
}

static int schedule_timer(bool repeating,
                          Uint32 delay_or_interval_ms,
                          MainThreadTimerCallback cb,
                          void* user_data,
                          const char* label) {
    if (!cb || !g_scheduler_initialized) return -1;
    if (delay_or_interval_ms == 0) delay_or_interval_ms = 1;

    int slot = alloc_timer_slot();
    if (slot < 0) return -1;

    uint64_t now_ns = loop_time_now_ns();
    uint64_t delay_ns = core_time_seconds_to_ns((double)delay_or_interval_ms / 1000.0);
    uint64_t deadline_ns = core_time_add_ns(now_ns, delay_ns);
    uint64_t interval_ns = repeating ? delay_ns : 0;

    CoreSchedTimerId sched_id = core_sched_add_timer(&g_sched,
                                                     deadline_ns,
                                                     interval_ns,
                                                     sched_fire_cb,
                                                     NULL);
    if (sched_id == 0) return -1;

    MainThreadTimerEntry* e = &g_timers[slot];
    memset(e, 0, sizeof(*e));
    e->id = g_next_timer_id++;
    e->in_use = true;
    e->repeating = repeating;
    e->interval_ns = interval_ns;
    e->sched_id = sched_id;
    e->cb = cb;
    e->user_data = user_data;
    if (label && label[0]) {
        snprintf(e->label, sizeof(e->label), "%s", label);
        e->label[sizeof(e->label) - 1] = '\0';
    }
    return e->id;
}

int mainthread_timer_schedule_once(Uint32 delay_ms,
                                   MainThreadTimerCallback cb,
                                   void* user_data,
                                   const char* label) {
    return schedule_timer(false, delay_ms, cb, user_data, label);
}

int mainthread_timer_schedule_repeating(Uint32 interval_ms,
                                        MainThreadTimerCallback cb,
                                        void* user_data,
                                        const char* label) {
    return schedule_timer(true, interval_ms, cb, user_data, label);
}

bool mainthread_timer_cancel(int timer_id) {
    MainThreadTimerEntry* e = find_timer_by_id(timer_id);
    if (!e) return false;
    if (e->sched_id != 0) {
        core_sched_cancel_timer(&g_sched, e->sched_id);
    }
    memset(e, 0, sizeof(*e));
    return true;
}

bool mainthread_timer_scheduler_next_deadline_ms(Uint32* out_deadline_ms) {
    if (!g_scheduler_initialized) return false;
    uint64_t now_ns = loop_time_now_ns();
    uint64_t next_ns = core_sched_next_deadline_ns(&g_sched, now_ns);
    if (next_ns == 0) return false;
    if (out_deadline_ms) {
        uint64_t ms = next_ns / 1000000ULL;
        if (ms > (uint64_t)UINT32_MAX) ms = (uint64_t)UINT32_MAX;
        *out_deadline_ms = (Uint32)ms;
    }
    return true;
}

int mainthread_timer_scheduler_fire_due(Uint32 now_ms) {
    (void)now_ms;
    if (!g_scheduler_initialized) return 0;
    uint64_t now_ns = loop_time_now_ns();
    return (int)core_sched_fire_due(&g_sched, now_ns, MAINTHREAD_TIMER_MAX);
}

void mainthread_timer_scheduler_snapshot(MainThreadTimerSchedulerStats* out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));

    uint32_t count = 0;
    for (int i = 0; i < MAINTHREAD_TIMER_MAX; ++i) {
        if (g_timers[i].in_use) count++;
    }
    out->timer_count = count;
    out->fired_count = g_fired_count;

    Uint32 next_deadline_ms = 0;
    out->has_deadline = mainthread_timer_scheduler_next_deadline_ms(&next_deadline_ms);
    out->next_deadline_ms = out->has_deadline ? next_deadline_ms : 0;
}

CoreSched* mainthread_timer_scheduler_get_core_sched(void) {
    if (!g_scheduler_initialized) return NULL;
    return &g_sched;
}
