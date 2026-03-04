#ifndef IDE_UI_EDITOR_NAVIGATION_H
#define IDE_UI_EDITOR_NAVIGATION_H

#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/project.h"
#include "ide/Panes/Editor/Commands/editor_commands.h"
#include "ide/Panes/Editor/editor_state.h"
#include "ide/Panes/Editor/editor_view.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static inline bool ui_editor_build_full_path(const char* path,
                                             char* out_path,
                                             size_t out_path_cap) {
    if (!out_path || out_path_cap == 0 || !path || !path[0]) return false;
    if (path[0] == '/') {
        snprintf(out_path, out_path_cap, "%s", path);
        return true;
    }
    if (!projectPath[0]) return false;
    snprintf(out_path, out_path_cap, "%s/%s", projectPath, path);
    return true;
}

static inline bool ui_editor_find_open_file_in_view_tree(EditorView* view,
                                                         const char* full_path,
                                                         EditorView** out_view,
                                                         int* out_tab_index) {
    if (!view || !full_path || !full_path[0]) return false;
    if (view->type == VIEW_LEAF) {
        for (int i = 0; i < view->fileCount; ++i) {
            OpenFile* file = view->openFiles ? view->openFiles[i] : NULL;
            if (file && file->filePath && strcmp(file->filePath, full_path) == 0) {
                if (out_view) *out_view = view;
                if (out_tab_index) *out_tab_index = i;
                return true;
            }
        }
        return false;
    }
    if (ui_editor_find_open_file_in_view_tree(view->childA, full_path, out_view, out_tab_index)) return true;
    if (ui_editor_find_open_file_in_view_tree(view->childB, full_path, out_view, out_tab_index)) return true;
    return false;
}

static inline EditorView* ui_pick_best_editor_view_for_path(const char* path) {
    IDECoreState* core = getCoreState();
    if (!core) return NULL;

    EditorView* root = core->persistentEditorView;
    EditorView* active = core->activeEditorView;
    char full_path[1024];
    if (root && ui_editor_build_full_path(path, full_path, sizeof(full_path))) {
        EditorView* matched = NULL;
        int matched_tab = -1;
        if (ui_editor_find_open_file_in_view_tree(root, full_path, &matched, &matched_tab)) {
            if (matched_tab >= 0) matched->activeTab = matched_tab;
            setActiveEditorView(matched);
            return matched;
        }
    }

    if (active && active->type == VIEW_LEAF) return active;
    if (root) {
        EditorView* first_leaf = findNextLeaf(root);
        if (first_leaf) {
            setActiveEditorView(first_leaf);
            return first_leaf;
        }
    }
    return NULL;
}

static inline OpenFile* ui_open_path_in_active_editor(const char* path) {
    IDECoreState* core = getCoreState();
    if (!core || !core->activeEditorView || !path || !path[0]) return NULL;

    char fullPath[1024];
    if (!ui_editor_build_full_path(path, fullPath, sizeof(fullPath))) {
        return NULL;
    }

    OpenFile* file = openFileInView(core->activeEditorView, fullPath);
    if (!file) return NULL;
    setActiveEditorView(core->activeEditorView);
    return file;
}

static inline bool ui_open_path_at_location_in_active_editor(const char* path,
                                                             int line,
                                                             int column) {
    OpenFile* file = ui_open_path_in_active_editor(path);
    if (!file || !file->buffer) return false;

    int targetRow = line > 0 ? line - 1 : 0;
    if (targetRow >= file->buffer->lineCount) targetRow = file->buffer->lineCount - 1;
    if (targetRow < 0) targetRow = 0;

    int lineLen = file->buffer->lines && file->buffer->lines[targetRow]
                      ? (int)strlen(file->buffer->lines[targetRow])
                      : 0;
    int targetCol = column > 0 ? column - 1 : 0;
    if (targetCol > lineLen) targetCol = lineLen;

    file->state.cursorRow = targetRow;
    file->state.cursorCol = targetCol;
    file->state.viewTopRow = (targetRow > 2) ? targetRow - 2 : 0;
    file->state.selecting = false;
    file->state.draggingWithMouse = false;
    return true;
}

static inline bool ui_open_path_at_location_in_best_editor_view(const char* path,
                                                                int line,
                                                                int column) {
    EditorView* view = ui_pick_best_editor_view_for_path(path);
    if (!view) return false;
    return editor_jump_to(view, path, line, column);
}

#endif
