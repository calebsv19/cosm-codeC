#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include <SDL2/SDL.h>

#include "core/LoopKernel/mainthread_context.h"

static int non_owner_scope_thread_fn(void* user_data) {
    bool* ran = (bool*)user_data;
    assert(ran != NULL);
    assert(!mainthread_context_is_owner_thread());
    mainthread_context_push_non_owner_scope();
    mainthread_context_assert_owner("mainthread_context_scope_regression_test.non_owner_scope");
    mainthread_context_pop_non_owner_scope();
    *ran = true;
    return 0;
}

int main(void) {
    mainthread_context_clear_owner();
    mainthread_context_set_owner_current();
    assert(mainthread_context_has_owner());

    bool ran = false;
    SDL_Thread* worker = SDL_CreateThread(non_owner_scope_thread_fn,
                                          "mainthread_context_scope_test",
                                          &ran);
    assert(worker != NULL);
    int thread_rc = 0;
    SDL_WaitThread(worker, &thread_rc);
    assert(thread_rc == 0);
    assert(ran);

    puts("mainthread_context_scope_regression_test: success");
    return 0;
}
