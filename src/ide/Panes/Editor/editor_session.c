#include "ide/Panes/Editor/editor_session.h"

#include <json-c/json.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "app/GlobalInfo/core_state.h"
#include "ide/Panes/Editor/editor_view.h"

static void ensure_ide_dir(const char* workspace_root) {
    if (!workspace_root || !*workspace_root) return;
    char dir[PATH_MAX];
    snprintf(dir, sizeof(dir), "%s/ide_files", workspace_root);
    struct stat st;
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        mkdir(dir, 0755);
    }
}

static void build_session_path(const char* workspace_root, char* out, size_t out_cap) {
    if (!out || out_cap == 0) return;
    if (!workspace_root || !*workspace_root) {
        out[0] = '\0';
        return;
    }
    snprintf(out, out_cap, "%s/ide_files/editor_session.json", workspace_root);
}

static const char* path_to_workspace_relative(const char* path, const char* workspace_root) {
    if (!path || !workspace_root || !*workspace_root) return path;
    size_t root_len = strlen(workspace_root);
    if (strncmp(path, workspace_root, root_len) != 0) return path;
    if (path[root_len] == '/') return path + root_len + 1;
    if (path[root_len] == '\0') return "";
    return path;
}

static void normalize_path_for_open(const char* workspace_root,
                                    const char* maybe_relative,
                                    char* out,
                                    size_t out_cap) {
    if (!out || out_cap == 0) return;
    out[0] = '\0';
    if (!maybe_relative || !*maybe_relative) return;
    if (maybe_relative[0] == '/') {
        snprintf(out, out_cap, "%s", maybe_relative);
        return;
    }
    snprintf(out, out_cap, "%s/%s", workspace_root ? workspace_root : "", maybe_relative);
}

int editor_session_count_leaves(const EditorView* root) {
    if (!root) return 0;
    if (root->type == VIEW_LEAF) return 1;
    return editor_session_count_leaves(root->childA) + editor_session_count_leaves(root->childB);
}

static json_object* serialize_node(const EditorView* node,
                                   const EditorView* active_leaf,
                                   const char* workspace_root,
                                   int* inout_leaf_index,
                                   int* out_active_leaf_index) {
    if (!node) return NULL;
    json_object* obj = json_object_new_object();
    if (node->type == VIEW_SPLIT) {
        json_object_object_add(obj, "type", json_object_new_string("split"));
        json_object_object_add(obj, "orientation",
                               json_object_new_string(node->splitType == SPLIT_VERTICAL
                                                          ? "vertical"
                                                          : "horizontal"));
        json_object* a = serialize_node(node->childA, active_leaf, workspace_root,
                                        inout_leaf_index, out_active_leaf_index);
        json_object* b = serialize_node(node->childB, active_leaf, workspace_root,
                                        inout_leaf_index, out_active_leaf_index);
        if (a) json_object_object_add(obj, "child_a", a);
        if (b) json_object_object_add(obj, "child_b", b);
        return obj;
    }

    int this_leaf_index = *inout_leaf_index;
    (*inout_leaf_index)++;
    if (node == active_leaf) {
        *out_active_leaf_index = this_leaf_index;
    }

    json_object_object_add(obj, "type", json_object_new_string("leaf"));
    json_object_object_add(obj, "leaf_index", json_object_new_int(this_leaf_index));
    json_object_object_add(obj, "active_tab", json_object_new_int(node->activeTab));

    json_object* files = json_object_new_array();
    for (int i = 0; i < node->fileCount; ++i) {
        OpenFile* f = node->openFiles ? node->openFiles[i] : NULL;
        if (!f || !f->filePath) continue;
        const char* rel = path_to_workspace_relative(f->filePath, workspace_root);
        json_object_array_add(files, json_object_new_string(rel ? rel : ""));
    }
    json_object_object_add(obj, "files", files);
    return obj;
}

bool editor_session_save(const char* workspace_root,
                         const EditorView* root,
                         const EditorView* active_leaf) {
    if (!workspace_root || !*workspace_root || !root) return false;
    ensure_ide_dir(workspace_root);

    json_object* payload = json_object_new_object();
    int leaf_index = 0;
    int active_leaf_index = -1;
    json_object* root_node = serialize_node(root, active_leaf, workspace_root,
                                            &leaf_index, &active_leaf_index);
    if (!root_node) {
        json_object_put(payload);
        return false;
    }

    json_object_object_add(payload, "version", json_object_new_int(1));
    json_object_object_add(payload, "active_leaf_index", json_object_new_int(active_leaf_index));
    json_object_object_add(payload, "root", root_node);

    char path[PATH_MAX];
    build_session_path(workspace_root, path, sizeof(path));
    FILE* fp = fopen(path, "w");
    if (!fp) {
        json_object_put(payload);
        return false;
    }
    const char* serialized = json_object_to_json_string_ext(payload, JSON_C_TO_STRING_PLAIN);
    if (serialized) {
        fputs(serialized, fp);
    }
    fclose(fp);
    json_object_put(payload);
    return true;
}

static EditorView* deserialize_node(json_object* node_obj,
                                    const char* workspace_root,
                                    int target_active_leaf_index,
                                    int* inout_leaf_index,
                                    EditorView** out_active_leaf) {
    if (!node_obj || !json_object_is_type(node_obj, json_type_object)) return NULL;

    json_object* jtype = NULL;
    if (!json_object_object_get_ex(node_obj, "type", &jtype)) return NULL;
    const char* type = json_object_get_string(jtype);
    if (!type) return NULL;

    if (strcmp(type, "split") == 0) {
        json_object* jori = NULL;
        json_object* ja = NULL;
        json_object* jb = NULL;
        json_object_object_get_ex(node_obj, "orientation", &jori);
        json_object_object_get_ex(node_obj, "child_a", &ja);
        json_object_object_get_ex(node_obj, "child_b", &jb);
        const char* ori = jori ? json_object_get_string(jori) : NULL;
        SplitOrientation split = (ori && strcmp(ori, "horizontal") == 0)
                                     ? SPLIT_HORIZONTAL
                                     : SPLIT_VERTICAL;
        EditorView* view = createSplitView(split);
        if (!view) return NULL;
        view->childA = deserialize_node(ja, workspace_root, target_active_leaf_index,
                                        inout_leaf_index, out_active_leaf);
        view->childB = deserialize_node(jb, workspace_root, target_active_leaf_index,
                                        inout_leaf_index, out_active_leaf);
        if (!view->childA || !view->childB) {
            destroyEditorView(view);
            return NULL;
        }
        return view;
    }

    if (strcmp(type, "leaf") != 0) return NULL;
    EditorView* leaf = createLeafView();
    if (!leaf) return NULL;

    json_object* jfiles = NULL;
    json_object* jactive = NULL;
    json_object_object_get_ex(node_obj, "files", &jfiles);
    json_object_object_get_ex(node_obj, "active_tab", &jactive);

    if (jfiles && json_object_is_type(jfiles, json_type_array)) {
        size_t count = json_object_array_length(jfiles);
        for (size_t i = 0; i < count; ++i) {
            json_object* jp = json_object_array_get_idx(jfiles, i);
            const char* persisted_path = jp ? json_object_get_string(jp) : NULL;
            if (!persisted_path || !*persisted_path) continue;
            char full[PATH_MAX];
            normalize_path_for_open(workspace_root, persisted_path, full, sizeof(full));
            if (!full[0]) continue;
            (void)openFileInView(leaf, full);
        }
    }

    int active_tab = jactive ? json_object_get_int(jactive) : -1;
    if (active_tab >= 0 && active_tab < leaf->fileCount) {
        leaf->activeTab = active_tab;
    }

    int this_leaf_index = *inout_leaf_index;
    (*inout_leaf_index)++;
    if (this_leaf_index == target_active_leaf_index) {
        *out_active_leaf = leaf;
    }
    return leaf;
}

bool editor_session_load(const char* workspace_root,
                         EditorView** out_root,
                         EditorView** out_active_leaf) {
    if (out_root) *out_root = NULL;
    if (out_active_leaf) *out_active_leaf = NULL;
    if (!workspace_root || !*workspace_root || !out_root || !out_active_leaf) return false;

    char path[PATH_MAX];
    build_session_path(workspace_root, path, sizeof(path));
    FILE* fp = fopen(path, "r");
    if (!fp) return false;

    fseek(fp, 0, SEEK_END);
    long len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (len <= 0 || len > (32 * 1024 * 1024)) {
        fclose(fp);
        return false;
    }

    char* buf = (char*)malloc((size_t)len + 1);
    if (!buf) {
        fclose(fp);
        return false;
    }
    fread(buf, 1, (size_t)len, fp);
    buf[len] = '\0';
    fclose(fp);

    json_object* payload = json_tokener_parse(buf);
    free(buf);
    if (!payload || !json_object_is_type(payload, json_type_object)) {
        if (payload) json_object_put(payload);
        return false;
    }

    json_object* jroot = NULL;
    json_object* jactive = NULL;
    json_object_object_get_ex(payload, "root", &jroot);
    json_object_object_get_ex(payload, "active_leaf_index", &jactive);
    int active_leaf_index = jactive ? json_object_get_int(jactive) : -1;

    IDECoreState* core = getCoreState();
    if (core) {
        core->persistentEditorView = NULL;
        core->activeEditorView = NULL;
    }

    int leaf_index = 0;
    EditorView* active_leaf = NULL;
    EditorView* root = deserialize_node(jroot, workspace_root, active_leaf_index,
                                        &leaf_index, &active_leaf);
    json_object_put(payload);
    if (!root) return false;

    *out_root = root;
    *out_active_leaf = active_leaf;
    return true;
}
