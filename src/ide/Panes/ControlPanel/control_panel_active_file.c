#include "ide/Panes/ControlPanel/control_panel_active_file.h"

#include <stdbool.h>

#include "app/GlobalInfo/core_state.h"
#include "ide/Panes/Editor/editor_view.h"

static bool file_has_path(const OpenFile* file) {
    return file && file->filePath && file->filePath[0] != '\0';
}

static const OpenFile* active_file_for_leaf(const EditorView* view) {
    if (!view || view->type != VIEW_LEAF) return NULL;
    if (view->activeTab < 0 || view->activeTab >= view->fileCount) return NULL;
    if (!view->openFiles) return NULL;

    OpenFile* file = view->openFiles[view->activeTab];
    return file_has_path(file) ? file : NULL;
}

static const OpenFile* first_active_file_in_tree(const EditorView* view) {
    if (!view) return NULL;

    if (view->type == VIEW_LEAF) {
        return active_file_for_leaf(view);
    }

    const OpenFile* file = first_active_file_in_tree(view->childA);
    if (file) return file;
    return first_active_file_in_tree(view->childB);
}

const OpenFile* control_panel_resolve_active_open_file(const IDECoreState* core) {
    if (!core) return NULL;

    const OpenFile* file = active_file_for_leaf(core->activeEditorView);
    if (file) return file;

    file = first_active_file_in_tree(core->activeEditorView);
    if (file) return file;

    return first_active_file_in_tree(core->persistentEditorView);
}

const char* control_panel_resolve_active_file_path(const IDECoreState* core) {
    const OpenFile* file = control_panel_resolve_active_open_file(core);
    return file_has_path(file) ? file->filePath : NULL;
}
