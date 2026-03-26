#include "core/LoopKernel/mainthread_context.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>

static Uint32 g_owner_thread_id = 0u;
#ifndef NDEBUG
static __thread int g_non_owner_scope_depth = 0;
#endif

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
    if (g_non_owner_scope_depth > 0) {
        return;
    }
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

void mainthread_context_push_non_owner_scope(void) {
#ifndef NDEBUG
    g_non_owner_scope_depth++;
#endif
}

void mainthread_context_pop_non_owner_scope(void) {
#ifndef NDEBUG
    if (g_non_owner_scope_depth > 0) {
        g_non_owner_scope_depth--;
    }
#endif
}
