#include "core/LoopTimer/mainthread_timer_scheduler.h"

#include <stdio.h>
#include <string.h>

#define MAINTHREAD_TIMER_MAX 64

typedef struct MainThreadTimerEntry {
    int id;
    bool in_use;
    bool repeating;
    Uint32 interval_ms;
    Uint32 due_ms;
    MainThreadTimerCallback cb;
    void* user_data;
    char label[32];
} MainThreadTimerEntry;

static MainThreadTimerEntry g_timers[MAINTHREAD_TIMER_MAX];
static int g_next_timer_id = 1;
static uint32_t g_fired_count = 0;

static MainThreadTimerEntry* find_timer_by_id(int timer_id) {
    if (timer_id <= 0) return NULL;
    for (int i = 0; i < MAINTHREAD_TIMER_MAX; ++i) {
        if (g_timers[i].in_use && g_timers[i].id == timer_id) {
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

void mainthread_timer_scheduler_init(void) {
    mainthread_timer_scheduler_reset();
}

void mainthread_timer_scheduler_shutdown(void) {
    mainthread_timer_scheduler_reset();
}

void mainthread_timer_scheduler_reset(void) {
    memset(g_timers, 0, sizeof(g_timers));
    g_next_timer_id = 1;
    g_fired_count = 0;
}

static int schedule_timer(bool repeating,
                          Uint32 delay_or_interval_ms,
                          MainThreadTimerCallback cb,
                          void* user_data,
                          const char* label) {
    if (!cb) return -1;
    if (delay_or_interval_ms == 0) delay_or_interval_ms = 1;

    int slot = alloc_timer_slot();
    if (slot < 0) return -1;

    MainThreadTimerEntry* e = &g_timers[slot];
    memset(e, 0, sizeof(*e));
    e->id = g_next_timer_id++;
    e->in_use = true;
    e->repeating = repeating;
    e->interval_ms = delay_or_interval_ms;
    e->due_ms = SDL_GetTicks() + delay_or_interval_ms;
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
    memset(e, 0, sizeof(*e));
    return true;
}

bool mainthread_timer_scheduler_next_deadline_ms(Uint32* out_deadline_ms) {
    bool found = false;
    Uint32 best = 0;
    for (int i = 0; i < MAINTHREAD_TIMER_MAX; ++i) {
        const MainThreadTimerEntry* e = &g_timers[i];
        if (!e->in_use) continue;
        if (!found || (int32_t)(e->due_ms - best) < 0) {
            best = e->due_ms;
            found = true;
        }
    }
    if (found && out_deadline_ms) {
        *out_deadline_ms = best;
    }
    return found;
}

int mainthread_timer_scheduler_fire_due(Uint32 now_ms) {
    int fired = 0;
    for (int i = 0; i < MAINTHREAD_TIMER_MAX; ++i) {
        MainThreadTimerEntry* e = &g_timers[i];
        if (!e->in_use || !e->cb) continue;
        if ((int32_t)(now_ms - e->due_ms) < 0) continue;

        MainThreadTimerCallback cb = e->cb;
        void* user_data = e->user_data;
        bool repeating = e->repeating;
        Uint32 interval_ms = e->interval_ms;

        if (repeating) {
            e->due_ms = now_ms + (interval_ms > 0 ? interval_ms : 1);
        } else {
            memset(e, 0, sizeof(*e));
        }

        cb(user_data);
        fired++;
        g_fired_count++;
    }
    return fired;
}

void mainthread_timer_scheduler_snapshot(MainThreadTimerSchedulerStats* out) {
    if (!out) return;
    memset(out, 0, sizeof(*out));

    uint32_t count = 0;
    Uint32 next_deadline = 0;
    bool have_deadline = false;
    for (int i = 0; i < MAINTHREAD_TIMER_MAX; ++i) {
        const MainThreadTimerEntry* e = &g_timers[i];
        if (!e->in_use) continue;
        count++;
        if (!have_deadline || (int32_t)(e->due_ms - next_deadline) < 0) {
            next_deadline = e->due_ms;
            have_deadline = true;
        }
    }

    out->timer_count = count;
    out->fired_count = g_fired_count;
    out->has_deadline = have_deadline;
    out->next_deadline_ms = have_deadline ? next_deadline : 0;
}
