#include <assert.h>
#include <string.h>

#include "app/GlobalInfo/core_state.h"
#include "ide/Panes/ControlPanel/control_panel_active_file.h"
#include "ide/Panes/Editor/editor_view.h"

static OpenFile file_with_path(const char* path) {
    OpenFile file = {0};
    file.filePath = (char*)path;
    return file;
}

static EditorView leaf_with_active_file(OpenFile* file, OpenFile** files) {
    files[0] = file;
    EditorView view = {0};
    view.type = VIEW_LEAF;
    view.openFiles = files;
    view.fileCount = 1;
    view.fileCapacity = 1;
    view.activeTab = 0;
    return view;
}

static EditorView empty_leaf(void) {
    EditorView view = {0};
    view.type = VIEW_LEAF;
    view.activeTab = -1;
    return view;
}

static void active_editor_view_wins(void) {
    OpenFile active_file = file_with_path("src/active.c");
    OpenFile persistent_file = file_with_path("src/persistent.c");
    OpenFile* active_files[1] = {0};
    OpenFile* persistent_files[1] = {0};
    EditorView active_leaf = leaf_with_active_file(&active_file, active_files);
    EditorView persistent_leaf = leaf_with_active_file(&persistent_file, persistent_files);
    IDECoreState core = {0};
    core.activeEditorView = &active_leaf;
    core.persistentEditorView = &persistent_leaf;

    const char* path = control_panel_resolve_active_file_path(&core);
    assert(path);
    assert(strcmp(path, "src/active.c") == 0);
}

static void split_active_view_falls_back_to_visible_leaf(void) {
    OpenFile split_file = file_with_path("src/split-child.c");
    OpenFile* split_files[1] = {0};
    EditorView left = empty_leaf();
    EditorView right = leaf_with_active_file(&split_file, split_files);
    EditorView split = {0};
    split.type = VIEW_SPLIT;
    split.childA = &left;
    split.childB = &right;

    IDECoreState core = {0};
    core.activeEditorView = &split;

    const char* path = control_panel_resolve_active_file_path(&core);
    assert(path);
    assert(strcmp(path, "src/split-child.c") == 0);
}

static void stale_active_view_falls_back_to_persistent_tree(void) {
    OpenFile persistent_file = file_with_path("src/main.c");
    OpenFile* persistent_files[1] = {0};
    EditorView stale_leaf = empty_leaf();
    EditorView persistent_leaf = leaf_with_active_file(&persistent_file, persistent_files);
    IDECoreState core = {0};
    core.activeEditorView = &stale_leaf;
    core.persistentEditorView = &persistent_leaf;

    const char* path = control_panel_resolve_active_file_path(&core);
    assert(path);
    assert(strcmp(path, "src/main.c") == 0);
}

static void invalid_paths_are_ignored(void) {
    OpenFile invalid_file = file_with_path("");
    OpenFile fallback_file = file_with_path("src/fallback.c");
    OpenFile* active_files[1] = {0};
    OpenFile* persistent_files[1] = {0};
    EditorView active_leaf = leaf_with_active_file(&invalid_file, active_files);
    EditorView persistent_leaf = leaf_with_active_file(&fallback_file, persistent_files);
    IDECoreState core = {0};
    core.activeEditorView = &active_leaf;
    core.persistentEditorView = &persistent_leaf;

    const char* path = control_panel_resolve_active_file_path(&core);
    assert(path);
    assert(strcmp(path, "src/fallback.c") == 0);
}

static void no_visible_file_returns_null(void) {
    EditorView active_leaf = empty_leaf();
    EditorView persistent_leaf = empty_leaf();
    IDECoreState core = {0};
    core.activeEditorView = &active_leaf;
    core.persistentEditorView = &persistent_leaf;

    assert(control_panel_resolve_active_file_path(&core) == NULL);
}

int main(void) {
    active_editor_view_wins();
    split_active_view_falls_back_to_visible_leaf();
    stale_active_view_falls_back_to_persistent_tree();
    invalid_paths_are_ignored();
    no_visible_file_returns_null();
    return 0;
}
