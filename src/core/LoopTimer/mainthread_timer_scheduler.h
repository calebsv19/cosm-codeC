#ifndef MAINTHREAD_TIMER_SCHEDULER_H
#define MAINTHREAD_TIMER_SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>
#include <SDL2/SDL.h>

typedef void (*MainThreadTimerCallback)(void* user_data);

typedef struct MainThreadTimerSchedulerStats {
    uint32_t timer_count;
    uint32_t fired_count;
    uint32_t next_deadline_ms;
    bool has_deadline;
} MainThreadTimerSchedulerStats;

void mainthread_timer_scheduler_init(void);
void mainthread_timer_scheduler_shutdown(void);
void mainthread_timer_scheduler_reset(void);

int mainthread_timer_schedule_once(Uint32 delay_ms,
                                   MainThreadTimerCallback cb,
                                   void* user_data,
                                   const char* label);
int mainthread_timer_schedule_repeating(Uint32 interval_ms,
                                        MainThreadTimerCallback cb,
                                        void* user_data,
                                        const char* label);
bool mainthread_timer_cancel(int timer_id);

bool mainthread_timer_scheduler_next_deadline_ms(Uint32* out_deadline_ms);
int mainthread_timer_scheduler_fire_due(Uint32 now_ms);
void mainthread_timer_scheduler_snapshot(MainThreadTimerSchedulerStats* out);

#endif // MAINTHREAD_TIMER_SCHEDULER_H

