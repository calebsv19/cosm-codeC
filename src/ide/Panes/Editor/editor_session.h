#ifndef EDITOR_SESSION_H
#define EDITOR_SESSION_H

#include <stdbool.h>

struct EditorView;

bool editor_session_save(const char* workspace_root,
                         const struct EditorView* root,
                         const struct EditorView* active_leaf);

bool editor_session_load(const char* workspace_root,
                         struct EditorView** out_root,
                         struct EditorView** out_active_leaf);

int editor_session_count_leaves(const struct EditorView* root);

#endif // EDITOR_SESSION_H
