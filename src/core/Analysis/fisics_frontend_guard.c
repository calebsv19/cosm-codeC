#include "core/Analysis/fisics_frontend_guard.h"

#include <SDL2/SDL.h>

static SDL_mutex* g_fisics_frontend_mutex = NULL;

static SDL_mutex* frontend_mutex_get_or_create(void) {
    if (g_fisics_frontend_mutex) return g_fisics_frontend_mutex;
    g_fisics_frontend_mutex = SDL_CreateMutex();
    return g_fisics_frontend_mutex;
}

void fisics_frontend_guard_lock(void) {
    SDL_mutex* mutex = frontend_mutex_get_or_create();
    if (!mutex) return;
    SDL_LockMutex(mutex);
}

void fisics_frontend_guard_unlock(void) {
    SDL_mutex* mutex = g_fisics_frontend_mutex;
    if (!mutex) return;
    SDL_UnlockMutex(mutex);
}
