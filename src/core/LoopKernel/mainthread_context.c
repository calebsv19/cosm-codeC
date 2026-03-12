#include "core/LoopKernel/mainthread_context.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>

static Uint32 g_owner_thread_id = 0u;

void mainthread_context_set_owner_current(void) {
    g_owner_thread_id = SDL_ThreadID();
}

void mainthread_context_clear_owner(void) {
    g_owner_thread_id = 0u;
}

bool mainthread_context_has_owner(void) {
    return g_owner_thread_id != 0u;
}

bool mainthread_context_is_owner_thread(void) {
    if (g_owner_thread_id == 0u) return true;
    return SDL_ThreadID() == g_owner_thread_id;
}

void mainthread_context_assert_owner(const char* scope) {
#ifndef NDEBUG
    if (g_owner_thread_id == 0u) {
        // Lazy binding keeps unit tests simple while still enforcing
        // single-owner semantics once any guarded path is exercised.
        g_owner_thread_id = SDL_ThreadID();
        return;
    }
    Uint32 current = SDL_ThreadID();
    if (current != g_owner_thread_id) {
        fprintf(stderr,
                "[MainThreadGuard] violation scope=%s owner=%u current=%u\n",
                scope ? scope : "(unknown)",
                (unsigned int)g_owner_thread_id,
                (unsigned int)current);
        abort();
    }
#else
    (void)scope;
#endif
}
