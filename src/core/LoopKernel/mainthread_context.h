#ifndef MAINTHREAD_CONTEXT_H
#define MAINTHREAD_CONTEXT_H

#include <stdbool.h>

void mainthread_context_set_owner_current(void);
void mainthread_context_clear_owner(void);
bool mainthread_context_has_owner(void);
bool mainthread_context_is_owner_thread(void);
void mainthread_context_assert_owner(const char* scope);
void mainthread_context_push_non_owner_scope(void);
void mainthread_context_pop_non_owner_scope(void);

#endif // MAINTHREAD_CONTEXT_H
