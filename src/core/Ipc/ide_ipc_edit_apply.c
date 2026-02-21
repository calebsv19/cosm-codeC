#include "core/Ipc/ide_ipc_edit_apply.h"

#include "app/GlobalInfo/core_state.h"
#include "core/BuildSystem/build_diagnostics.h"
#include "core/CommandBus/save_queue.h"
#include "core/Diagnostics/diagnostics_engine.h"
#include "ide/Panes/Editor/editor_buffer.h"
#include "ide/Panes/Editor/editor_view.h"
#include "ide/Panes/Editor/undo_stack.h"

#include <json-c/json.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_PATCH_FILES 128
#define MAX_HUNK_LINES 65536

typedef struct {
    char op;      // ' ', '+', '-'
    char* text;   // line content without prefix
} PatchHunkLine;

typedef struct {
    int old_start;
    int old_count;
    int new_start;
    int new_count;
    PatchHunkLine* lines;
    size_t line_count;
    size_t line_cap;
} PatchHunk;

typedef struct {
    char* old_path;
    char* new_path;
    PatchHunk* hunks;
    size_t hunk_count;
    size_t hunk_cap;
} FilePatch;

typedef struct {
    FilePatch* files;
    size_t file_count;
    size_t file_cap;
} UnifiedPatch;

typedef struct {
    char** items;
    size_t count;
    size_t cap;
} StrList;

static void set_error(char* out, size_t cap, const char* msg) {
    if (!out || cap == 0) return;
    snprintf(out, cap, "%s", msg ? msg : "Unknown error");
}

static void hash_to_hex(unsigned long long hash, char* out, size_t cap) {
    if (!out || cap < 17) return;
    snprintf(out, cap, "%016llx", hash);
}

bool ide_ipc_compute_file_hash_hex(const char* path, char* out_hex, size_t out_hex_cap) {
    if (!path || !*path || !out_hex || out_hex_cap < 17) return false;
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    unsigned long long hash = 1469598103934665603ULL;
    unsigned char buf[4096];
    for (;;) {
        size_t n = fread(buf, 1, sizeof(buf), f);
        if (n > 0) {
            for (size_t i = 0; i < n; ++i) {
                hash ^= (unsigned long long)buf[i];
                hash *= 1099511628211ULL;
            }
        }
        if (n < sizeof(buf)) break;
    }
    fclose(f);
    hash_to_hex(hash, out_hex, out_hex_cap);
    return true;
}

static char* strdup_safe(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* out = (char*)malloc(len);
    if (!out) return NULL;
    memcpy(out, s, len);
    return out;
}

static void trim_path_token(const char* in, char* out, size_t out_cap) {
    if (!in || !*in || !out || out_cap == 0) {
        if (out && out_cap) out[0] = '\0';
        return;
    }
    while (*in == ' ' || *in == '\t') in++;
    if (strncmp(in, "a/", 2) == 0 || strncmp(in, "b/", 2) == 0) in += 2;
    const char* end = in;
    while (*end && *end != '\t' && *end != ' ') end++;
    size_t len = (size_t)(end - in);
    if (len >= out_cap) len = out_cap - 1;
    memcpy(out, in, len);
    out[len] = '\0';
}

static void free_patch(UnifiedPatch* patch) {
    if (!patch) return;
    for (size_t i = 0; i < patch->file_count; ++i) {
        FilePatch* fp = &patch->files[i];
        free(fp->old_path);
        free(fp->new_path);
        for (size_t h = 0; h < fp->hunk_count; ++h) {
            PatchHunk* hk = &fp->hunks[h];
            for (size_t l = 0; l < hk->line_count; ++l) {
                free(hk->lines[l].text);
            }
            free(hk->lines);
        }
        free(fp->hunks);
    }
    free(patch->files);
    patch->files = NULL;
    patch->file_count = 0;
    patch->file_cap = 0;
}

static FilePatch* patch_add_file(UnifiedPatch* patch) {
    if (!patch) return NULL;
    if (patch->file_count >= patch->file_cap) {
        size_t new_cap = patch->file_cap ? patch->file_cap * 2 : 8;
        FilePatch* tmp = (FilePatch*)realloc(patch->files, new_cap * sizeof(FilePatch));
        if (!tmp) return NULL;
        patch->files = tmp;
        patch->file_cap = new_cap;
    }
    FilePatch* fp = &patch->files[patch->file_count++];
    memset(fp, 0, sizeof(*fp));
    return fp;
}

static PatchHunk* file_add_hunk(FilePatch* fp) {
    if (!fp) return NULL;
    if (fp->hunk_count >= fp->hunk_cap) {
        size_t new_cap = fp->hunk_cap ? fp->hunk_cap * 2 : 8;
        PatchHunk* tmp = (PatchHunk*)realloc(fp->hunks, new_cap * sizeof(PatchHunk));
        if (!tmp) return NULL;
        fp->hunks = tmp;
        fp->hunk_cap = new_cap;
    }
    PatchHunk* hk = &fp->hunks[fp->hunk_count++];
    memset(hk, 0, sizeof(*hk));
    return hk;
}

static bool hunk_add_line(PatchHunk* hk, char op, const char* text) {
    if (!hk || !text) return false;
    if (hk->line_count >= hk->line_cap) {
        size_t new_cap = hk->line_cap ? hk->line_cap * 2 : 32;
        PatchHunkLine* tmp = (PatchHunkLine*)realloc(hk->lines, new_cap * sizeof(PatchHunkLine));
        if (!tmp) return false;
        hk->lines = tmp;
        hk->line_cap = new_cap;
    }
    PatchHunkLine* ln = &hk->lines[hk->line_count++];
    ln->op = op;
    ln->text = strdup_safe(text);
    return (ln->text != NULL);
}

static bool parse_hunk_header(const char* line, PatchHunk* out) {
    if (!line || !out) return false;
    // @@ -oldStart,oldCount +newStart,newCount @@
    int os = 0, oc = 1, ns = 0, nc = 1;
    int matched = sscanf(line, "@@ -%d,%d +%d,%d @@", &os, &oc, &ns, &nc);
    if (matched < 4) {
        oc = 1;
        nc = 1;
        matched = sscanf(line, "@@ -%d +%d @@", &os, &ns);
        if (matched < 2) return false;
    }
    out->old_start = os;
    out->old_count = oc;
    out->new_start = ns;
    out->new_count = nc;
    return true;
}

static bool parse_unified_diff(const char* diff_text, UnifiedPatch* out_patch, char* error_out, size_t error_cap) {
    if (!diff_text || !*diff_text || !out_patch) {
        set_error(error_out, error_cap, "Empty diff");
        return false;
    }

    memset(out_patch, 0, sizeof(*out_patch));
    char* text = strdup_safe(diff_text);
    if (!text) {
        set_error(error_out, error_cap, "Out of memory");
        return false;
    }

    FilePatch* current_file = NULL;
    PatchHunk* current_hunk = NULL;

    char* save = NULL;
    for (char* line = strtok_r(text, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        if (strncmp(line, "--- ", 4) == 0) {
            current_file = patch_add_file(out_patch);
            if (!current_file) {
                free(text);
                set_error(error_out, error_cap, "Out of memory creating file patch");
                free_patch(out_patch);
                return false;
            }
            char path[1024];
            trim_path_token(line + 4, path, sizeof(path));
            current_file->old_path = strdup_safe(path);
            current_hunk = NULL;
            continue;
        }
        if (strncmp(line, "+++ ", 4) == 0) {
            if (!current_file) continue;
            char path[1024];
            trim_path_token(line + 4, path, sizeof(path));
            current_file->new_path = strdup_safe(path);
            continue;
        }
        if (strncmp(line, "@@ ", 3) == 0) {
            if (!current_file) continue;
            current_hunk = file_add_hunk(current_file);
            if (!current_hunk || !parse_hunk_header(line, current_hunk)) {
                free(text);
                set_error(error_out, error_cap, "Malformed hunk header");
                free_patch(out_patch);
                return false;
            }
            continue;
        }
        if (!current_hunk) continue;
        if (line[0] == '\\') {
            // "\ No newline at end of file" marker: ignore
            continue;
        }
        if ((line[0] == ' ' || line[0] == '+' || line[0] == '-') && line[1] != '\0') {
            if (!hunk_add_line(current_hunk, line[0], line + 1)) {
                free(text);
                set_error(error_out, error_cap, "Out of memory adding hunk line");
                free_patch(out_patch);
                return false;
            }
        } else if (line[0] == ' ' || line[0] == '+' || line[0] == '-') {
            if (!hunk_add_line(current_hunk, line[0], "")) {
                free(text);
                set_error(error_out, error_cap, "Out of memory adding empty hunk line");
                free_patch(out_patch);
                return false;
            }
        }
    }

    free(text);
    if (out_patch->file_count == 0) {
        set_error(error_out, error_cap, "No file patches found");
        free_patch(out_patch);
        return false;
    }

    for (size_t i = 0; i < out_patch->file_count; ++i) {
        FilePatch* fp = &out_patch->files[i];
        if (!fp->new_path || !*fp->new_path || strcmp(fp->new_path, "/dev/null") == 0) {
            set_error(error_out, error_cap, "Only modifications to existing files are supported");
            free_patch(out_patch);
            return false;
        }
    }

    return true;
}

static void free_str_list(StrList* list) {
    if (!list) return;
    for (size_t i = 0; i < list->count; ++i) free(list->items[i]);
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

static bool split_lines(const char* text, StrList* out) {
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    const char* p = text ? text : "";
    const char* start = p;
    while (*p) {
        if (*p == '\n') {
            size_t len = (size_t)(p - start);
            char* line = (char*)malloc(len + 1);
            if (!line) { free_str_list(out); return false; }
            memcpy(line, start, len);
            line[len] = '\0';
            if (out->count >= out->cap) {
                size_t nc = out->cap ? out->cap * 2 : 32;
                char** tmp = (char**)realloc(out->items, nc * sizeof(char*));
                if (!tmp) { free(line); free_str_list(out); return false; }
                out->items = tmp;
                out->cap = nc;
            }
            out->items[out->count++] = line;
            start = p + 1;
        }
        p++;
    }
    // Last line (even empty)
    size_t len = (size_t)(p - start);
    char* line = (char*)malloc(len + 1);
    if (!line) { free_str_list(out); return false; }
    memcpy(line, start, len);
    line[len] = '\0';
    if (out->count >= out->cap) {
        size_t nc = out->cap ? out->cap * 2 : 32;
        char** tmp = (char**)realloc(out->items, nc * sizeof(char*));
        if (!tmp) { free(line); free_str_list(out); return false; }
        out->items = tmp;
        out->cap = nc;
    }
    out->items[out->count++] = line;
    return true;
}

static char* join_lines(const StrList* lines, size_t* out_len) {
    if (!lines) return NULL;
    size_t total = 0;
    for (size_t i = 0; i < lines->count; ++i) {
        total += strlen(lines->items[i]);
        if (i + 1 < lines->count) total += 1;
    }
    char* out = (char*)malloc(total + 1);
    if (!out) return NULL;
    size_t off = 0;
    for (size_t i = 0; i < lines->count; ++i) {
        size_t len = strlen(lines->items[i]);
        memcpy(out + off, lines->items[i], len);
        off += len;
        if (i + 1 < lines->count) out[off++] = '\n';
    }
    out[off] = '\0';
    if (out_len) *out_len = off;
    return out;
}

static bool append_line(StrList* list, const char* line) {
    if (!list || !line) return false;
    if (list->count >= list->cap) {
        size_t nc = list->cap ? list->cap * 2 : 32;
        char** tmp = (char**)realloc(list->items, nc * sizeof(char*));
        if (!tmp) return false;
        list->items = tmp;
        list->cap = nc;
    }
    list->items[list->count] = strdup_safe(line);
    if (!list->items[list->count]) return false;
    list->count++;
    return true;
}

static bool apply_file_patch_to_text(const char* original,
                                     const FilePatch* patch,
                                     char** out_text,
                                     char* error_out,
                                     size_t error_cap) {
    if (!patch || !out_text) return false;
    *out_text = NULL;

    StrList src = {0};
    StrList dst = {0};
    if (!split_lines(original ? original : "", &src)) {
        set_error(error_out, error_cap, "Out of memory splitting source");
        return false;
    }

    size_t src_idx = 0;
    for (size_t h = 0; h < patch->hunk_count; ++h) {
        const PatchHunk* hk = &patch->hunks[h];
        size_t target_idx = (hk->old_start > 0) ? (size_t)(hk->old_start - 1) : 0;
        if (target_idx > src.count) target_idx = src.count;

        while (src_idx < target_idx && src_idx < src.count) {
            if (!append_line(&dst, src.items[src_idx])) {
                free_str_list(&src); free_str_list(&dst);
                set_error(error_out, error_cap, "Out of memory building patched output");
                return false;
            }
            src_idx++;
        }

        for (size_t li = 0; li < hk->line_count; ++li) {
            const PatchHunkLine* pl = &hk->lines[li];
            if (pl->op == ' ') {
                if (src_idx >= src.count || strcmp(src.items[src_idx], pl->text) != 0) {
                    free_str_list(&src); free_str_list(&dst);
                    set_error(error_out, error_cap, "Patch context mismatch");
                    return false;
                }
                if (!append_line(&dst, src.items[src_idx])) {
                    free_str_list(&src); free_str_list(&dst);
                    set_error(error_out, error_cap, "Out of memory appending context line");
                    return false;
                }
                src_idx++;
            } else if (pl->op == '-') {
                if (src_idx >= src.count || strcmp(src.items[src_idx], pl->text) != 0) {
                    free_str_list(&src); free_str_list(&dst);
                    set_error(error_out, error_cap, "Patch removal mismatch");
                    return false;
                }
                src_idx++;
            } else if (pl->op == '+') {
                if (!append_line(&dst, pl->text)) {
                    free_str_list(&src); free_str_list(&dst);
                    set_error(error_out, error_cap, "Out of memory appending added line");
                    return false;
                }
            }
        }
    }

    while (src_idx < src.count) {
        if (!append_line(&dst, src.items[src_idx])) {
            free_str_list(&src); free_str_list(&dst);
            set_error(error_out, error_cap, "Out of memory appending tail lines");
            return false;
        }
        src_idx++;
    }

    size_t out_len = 0;
    char* merged = join_lines(&dst, &out_len);
    free_str_list(&src);
    free_str_list(&dst);
    if (!merged) {
        set_error(error_out, error_cap, "Out of memory joining patched text");
        return false;
    }
    *out_text = merged;
    return true;
}

static char* read_text_file(const char* path, size_t* out_len) {
    if (out_len) *out_len = 0;
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 0 || len > (64 * 1024 * 1024)) {
        fclose(f);
        return NULL;
    }
    char* buf = (char*)malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t n = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[n] = '\0';
    if (out_len) *out_len = n;
    return buf;
}

static EditorView* resolve_leaf_editor_view(void) {
    IDECoreState* core = getCoreState();
    if (!core) return NULL;
    EditorView* view = core->activeEditorView;
    if (!view) {
        view = core->persistentEditorView;
        if (view && view->type != VIEW_LEAF) {
            view = findNextLeaf(view);
        }
        if (view) {
            setActiveEditorView(view);
        }
    }
    return view;
}

static OpenFile* open_or_get_file(EditorView* view, const char* abs_path) {
    if (!view || !abs_path) return NULL;
    return openFileInView(view, abs_path);
}

static void diagnostics_summary(json_object* out_summary) {
    int total = 0, err = 0, warn = 0, info = 0;
    size_t bcount = 0;
    const BuildDiagnostic* b = build_diagnostics_get(&bcount);
    for (size_t i = 0; i < bcount; ++i) {
        total++;
        if (b[i].isError) err++; else warn++;
    }
    int dcount = getDiagnosticCount();
    for (int i = 0; i < dcount; ++i) {
        const Diagnostic* d = getDiagnosticAt(i);
        if (!d) continue;
        total++;
        if (d->severity == DIAG_SEVERITY_ERROR) err++;
        else if (d->severity == DIAG_SEVERITY_WARNING) warn++;
        else info++;
    }
    json_object_object_add(out_summary, "total", json_object_new_int(total));
    json_object_object_add(out_summary, "error", json_object_new_int(err));
    json_object_object_add(out_summary, "warn", json_object_new_int(warn));
    json_object_object_add(out_summary, "info", json_object_new_int(info));
}

bool ide_ipc_apply_unified_diff(const char* project_root,
                                const char* diff_text,
                                json_object** result_out,
                                char* error_out,
                                size_t error_out_cap) {
    if (result_out) *result_out = NULL;
    if (!project_root || !*project_root || !diff_text || !*diff_text) {
        set_error(error_out, error_out_cap, "Missing project root or diff text");
        return false;
    }

    UnifiedPatch patch = {0};
    if (!parse_unified_diff(diff_text, &patch, error_out, error_out_cap)) {
        return false;
    }

    EditorView* view = resolve_leaf_editor_view();
    if (!view) {
        free_patch(&patch);
        set_error(error_out, error_out_cap, "No editor view available for patch apply");
        return false;
    }

    json_object* touched = json_object_new_array();

    for (size_t i = 0; i < patch.file_count; ++i) {
        FilePatch* fp = &patch.files[i];
        if (!fp->new_path || strcmp(fp->new_path, "/dev/null") == 0) {
            free_patch(&patch);
            set_error(error_out, error_out_cap, "Unsupported patch target path");
            json_object_put(touched);
            return false;
        }

        char abs_path[1024];
        if (fp->new_path[0] == '/') {
            snprintf(abs_path, sizeof(abs_path), "%s", fp->new_path);
        } else {
            snprintf(abs_path, sizeof(abs_path), "%s/%s", project_root, fp->new_path);
        }

        size_t old_len = 0;
        char* old_text = read_text_file(abs_path, &old_len);
        if (!old_text) {
            free_patch(&patch);
            set_error(error_out, error_out_cap, "Failed to read target file for patch");
            json_object_put(touched);
            return false;
        }

        char* new_text = NULL;
        if (!apply_file_patch_to_text(old_text, fp, &new_text, error_out, error_out_cap)) {
            free(old_text);
            free_patch(&patch);
            json_object_put(touched);
            return false;
        }
        free(old_text);

        OpenFile* file = open_or_get_file(view, abs_path);
        if (!file || !file->buffer) {
            free(new_text);
            free_patch(&patch);
            set_error(error_out, error_out_cap, "Failed to open file buffer for patch apply");
            json_object_put(touched);
            return false;
        }

        pushUndoState(file);
        loadSnapshotIntoBuffer(file->buffer, new_text, strlen(new_text));
        markFileAsModified(file);
        queueSaveFromOpenFile(file);
        free(new_text);

        json_object_array_add(touched, json_object_new_string(fp->new_path));
    }

    int guard = 0;
    while (isSaveQueueBusy() && guard < 4096) {
        tickSaveQueue();
        guard++;
    }

    json_object* result = json_object_new_object();
    json_object_object_add(result, "applied", json_object_new_boolean(true));
    json_object_object_add(result, "touched_files", touched);
    json_object* summary = json_object_new_object();
    diagnostics_summary(summary);
    json_object_object_add(result, "diagnostics_summary", summary);

    free_patch(&patch);
    if (result_out) *result_out = result;
    return true;
}
