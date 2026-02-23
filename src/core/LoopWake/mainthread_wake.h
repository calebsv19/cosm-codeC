#ifndef MAINTHREAD_WAKE_H
#define MAINTHREAD_WAKE_H

#include <stdbool.h>
#include <stdint.h>
#include <SDL2/SDL.h>

typedef struct MainThreadWakeStats {
    uint32_t event_type;
    uint32_t pushes;
    uint32_t received;
    uint32_t push_failures;
} MainThreadWakeStats;

bool mainthread_wake_init(void);
void mainthread_wake_shutdown(void);
bool mainthread_wake_push(void);
bool mainthread_wake_is_event(const SDL_Event* event);
void mainthread_wake_note_received(void);
void mainthread_wake_snapshot(MainThreadWakeStats* out);

#endif // MAINTHREAD_WAKE_H

