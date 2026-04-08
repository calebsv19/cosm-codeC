#include "core/Analysis/analysis_store.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#include "core/LoopKernel/mainthread_context.h"

static AnalysisFileDiagnostics* g_files = NULL;
static size_t g_file_count = 0;
static size_t g_file_cap = 0;
static uint64_t g_stamp_counter = 0;
static uint64_t g_published_stamp = 0;
static pthread_mutex_t g_analysis_store_mutex = PTHREAD_MUTEX_INITIALIZER;

void analysis_store_lock(void) {
    pthread_mutex_lock(&g_analysis_store_mutex);
}

void analysis_store_unlock(void) {
    pthread_mutex_unlock(&g_analysis_store_mutex);
}

static void free_entry(AnalysisFileDiagnostics* f) {
    if (!f) return;
    free(f->path);
    for (int i = 0; i < f->count; ++i) {
        free((char*)f->diags[i].filePath);
        free((char*)f->diags[i].message);
    }
    free(f->diags);
    f->path = NULL;
    f->diags = NULL;
    f->count = 0;
}

static void analysis_store_clear_locked(void) {
    for (size_t i = 0; i < g_file_count; ++i) {
        free_entry(&g_files[i]);
    }
    free(g_files);
    g_files = NULL;
    g_file_count = 0;
    g_file_cap = 0;
    g_stamp_counter = 0;
    g_published_stamp = 0;
}

void analysis_store_clear(void) {
    mainthread_context_assert_owner("analysis_store.clear");
    analysis_store_lock();
    analysis_store_clear_locked();
    analysis_store_unlock();
}

static DiagnosticSeverity map_severity(DiagKind kind) {
    switch (kind) {
        case DIAG_WARNING: return DIAG_SEVERITY_WARNING;
        case DIAG_NOTE:    return DIAG_SEVERITY_INFO;
        case DIAG_ERROR:
        default:           return DIAG_SEVERITY_ERROR;
    }
}

static DiagnosticCategory map_category(int category_id) {
    switch (category_id) {
        case FISICS_DIAG_CATEGORY_ANALYSIS: return DIAG_CATEGORY_ANALYSIS;
        case FISICS_DIAG_CATEGORY_PARSER: return DIAG_CATEGORY_PARSER;
        case FISICS_DIAG_CATEGORY_SEMANTIC: return DIAG_CATEGORY_SEMANTIC;
        case FISICS_DIAG_CATEGORY_PREPROCESSOR: return DIAG_CATEGORY_PREPROCESSOR;
        case FISICS_DIAG_CATEGORY_LEXER: return DIAG_CATEGORY_LEXER;
        case FISICS_DIAG_CATEGORY_CODEGEN: return DIAG_CATEGORY_CODEGEN;
        case FISICS_DIAG_CATEGORY_BUILD: return DIAG_CATEGORY_BUILD;
        case FISICS_DIAG_CATEGORY_UNKNOWN:
        default: return DIAG_CATEGORY_UNKNOWN;
    }
}

static int map_fisics_category(DiagnosticCategory category) {
    switch (category) {
        case DIAG_CATEGORY_ANALYSIS: return FISICS_DIAG_CATEGORY_ANALYSIS;
        case DIAG_CATEGORY_PARSER: return FISICS_DIAG_CATEGORY_PARSER;
        case DIAG_CATEGORY_SEMANTIC: return FISICS_DIAG_CATEGORY_SEMANTIC;
        case DIAG_CATEGORY_PREPROCESSOR: return FISICS_DIAG_CATEGORY_PREPROCESSOR;
        case DIAG_CATEGORY_LEXER: return FISICS_DIAG_CATEGORY_LEXER;
        case DIAG_CATEGORY_CODEGEN: return FISICS_DIAG_CATEGORY_CODEGEN;
        case DIAG_CATEGORY_BUILD: return FISICS_DIAG_CATEGORY_BUILD;
        case DIAG_CATEGORY_UNKNOWN:
        default: return FISICS_DIAG_CATEGORY_UNKNOWN;
    }
}

static void analysis_store_upsert_locked(const char* filePath,
                                         const FisicsDiagnostic* fisicsDiags,
                                         size_t diagCount) {
    if (!filePath) return;

    size_t existing = (size_t)-1;
    for (size_t i = 0; i < g_file_count; ++i) {
        if (g_files[i].path && strcmp(g_files[i].path, filePath) == 0) {
            existing = i;
            break;
        }
    }
    if (existing != (size_t)-1) {
        free_entry(&g_files[existing]);
        for (size_t j = existing + 1; j < g_file_count; ++j) {
            g_files[j - 1] = g_files[j];
        }
        g_file_count--;
    }

    if (g_file_count >= g_file_cap) {
        size_t newCap = g_file_cap ? g_file_cap * 2 : 8;
        AnalysisFileDiagnostics* tmp = realloc(g_files, newCap * sizeof(AnalysisFileDiagnostics));
        if (!tmp) return;
        g_files = tmp;
        g_file_cap = newCap;
    }

    AnalysisFileDiagnostics entry = {0};
    entry.path = strdup(filePath);
    entry.count = (int)diagCount;
    entry.stamp = ++g_stamp_counter;

    if (diagCount > 0) {
        entry.diags = calloc(diagCount, sizeof(Diagnostic));
        if (!entry.diags) {
            free(entry.path);
            return;
        }
        for (size_t i = 0; i < diagCount; ++i) {
            // Defensive: do not trust pointers inside fisicsDiags (may be freed by frontend teardown).
            entry.diags[i].filePath = strdup(filePath);
            entry.diags[i].line = fisicsDiags[i].line;
            entry.diags[i].column = fisicsDiags[i].column;
            const char* msg = (fisicsDiags[i].message && fisicsDiags[i].message[0])
                              ? fisicsDiags[i].message
                              : "(no message)";
            entry.diags[i].message = strdup(msg);
            entry.diags[i].severity = map_severity(fisicsDiags[i].kind);
            entry.diags[i].category = map_category(fisicsDiags[i].category_id);
            entry.diags[i].codeId = fisicsDiags[i].code_id;
        }
    }

    // Insert at front to keep newest-first ordering
    for (size_t j = g_file_count; j > 0; --j) {
        g_files[j] = g_files[j - 1];
    }
    g_files[0] = entry;
    g_file_count++;
}

void analysis_store_upsert(const char* filePath,
                           const FisicsDiagnostic* fisicsDiags,
                           size_t diagCount) {
    mainthread_context_assert_owner("analysis_store.upsert");
    analysis_store_lock();
    analysis_store_upsert_locked(filePath, fisicsDiags, diagCount);
    analysis_store_unlock();
}

void analysis_store_remove(const char* filePath) {
    if (!filePath) return;
    mainthread_context_assert_owner("analysis_store.remove");
    analysis_store_lock();
    size_t existing = (size_t)-1;
    for (size_t i = 0; i < g_file_count; ++i) {
        if (g_files[i].path && strcmp(g_files[i].path, filePath) == 0) {
            existing = i;
            break;
        }
    }
    if (existing != (size_t)-1) {
        free_entry(&g_files[existing]);
        for (size_t j = existing + 1; j < g_file_count; ++j) {
            g_files[j - 1] = g_files[j];
        }
        g_file_count--;
        // Removal is a state change and must advance the stamp so stale guards
        // can differentiate deletion-only updates from prior snapshots.
        g_stamp_counter++;
    }
    analysis_store_unlock();
}

size_t analysis_store_file_count(void) {
    return g_file_count;
}

const AnalysisFileDiagnostics* analysis_store_file_at(size_t idx) {
    if (idx >= g_file_count) return NULL;
    return &g_files[idx];
}

uint64_t analysis_store_combined_stamp(void) {
    analysis_store_lock();
    uint64_t stamp = g_stamp_counter;
    analysis_store_unlock();
    return stamp;
}

uint64_t analysis_store_published_stamp(void) {
    analysis_store_lock();
    uint64_t stamp = g_published_stamp;
    analysis_store_unlock();
    return stamp;
}

void analysis_store_mark_published(uint64_t stamp) {
    mainthread_context_assert_owner("analysis_store.mark_published");
    analysis_store_lock();
    g_published_stamp = stamp;
    analysis_store_unlock();
}

static void analysis_store_flatten_to_engine_locked(void) {
    clearDiagnostics();
    for (size_t i = 0; i < g_file_count; ++i) {
        AnalysisFileDiagnostics* f = &g_files[i];
        for (int d = 0; d < f->count; ++d) {
            addDiagnosticWithMeta(f->diags[d].filePath ? f->diags[d].filePath : f->path,
                                  f->diags[d].line,
                                  f->diags[d].column,
                                  f->diags[d].message ? f->diags[d].message : "(no message)",
                                  f->diags[d].severity,
                                  f->diags[d].category,
                                  f->diags[d].codeId);
        }
    }
}

void analysis_store_flatten_to_engine(void) {
    mainthread_context_assert_owner("analysis_store.flatten_to_engine");
    analysis_store_lock();
    analysis_store_flatten_to_engine_locked();
    analysis_store_unlock();
}

// Persistence helpers
#include <json-c/json.h>
#include <sys/stat.h>

static void ensure_cache_dir(const char* workspaceRoot) {
    if (!workspaceRoot || !*workspaceRoot) return;
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s/ide_files", workspaceRoot);
    struct stat st;
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        mkdir(dir, 0755);
    }
}

void analysis_store_save(const char* workspaceRoot) {
    if (!workspaceRoot || !*workspaceRoot) return;
    ensure_cache_dir(workspaceRoot);
    char path[1024];
    snprintf(path, sizeof(path), "%s/ide_files/analysis_diagnostics.json", workspaceRoot);

    analysis_store_lock();
    json_object* arr = json_object_new_array();
    for (size_t i = 0; i < g_file_count; ++i) {
        AnalysisFileDiagnostics* f = &g_files[i];
        json_object* obj = json_object_new_object();
        json_object_object_add(obj, "path", json_object_new_string(f->path ? f->path : ""));
        json_object_object_add(obj, "stamp", json_object_new_int64((long long)f->stamp));
        json_object* diags = json_object_new_array();
        for (int d = 0; d < f->count; ++d) {
            json_object* jd = json_object_new_object();
            json_object_object_add(jd, "line", json_object_new_int(f->diags[d].line));
            json_object_object_add(jd, "col", json_object_new_int(f->diags[d].column));
            json_object_object_add(jd, "severity", json_object_new_int(f->diags[d].severity));
            json_object_object_add(jd, "category", json_object_new_int((int)f->diags[d].category));
            json_object_object_add(jd, "code_id", json_object_new_int(f->diags[d].codeId));
            json_object_object_add(jd, "message", json_object_new_string(f->diags[d].message ? f->diags[d].message : ""));
            json_object_array_add(diags, jd);
        }
        json_object_object_add(obj, "diagnostics", diags);
        json_object_array_add(arr, obj);
    }

    const char* serialized = json_object_to_json_string_ext(arr, JSON_C_TO_STRING_PLAIN);
    FILE* f = fopen(path, "w");
    if (f && serialized) {
        fputs(serialized, f);
        fclose(f);
    } else if (f) {
        fclose(f);
    }
    json_object_put(arr);
    analysis_store_unlock();
}

void analysis_store_load(const char* workspaceRoot) {
    mainthread_context_assert_owner("analysis_store.load");
    analysis_store_lock();
    analysis_store_clear_locked();
    if (!workspaceRoot || !*workspaceRoot) {
        analysis_store_unlock();
        return;
    }
    char path[1024];
    snprintf(path, sizeof(path), "%s/ide_files/analysis_diagnostics.json", workspaceRoot);
    FILE* f = fopen(path, "r");
    if (!f) {
        analysis_store_unlock();
        return;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0 || len > (32 * 1024 * 1024)) {
        fclose(f);
        analysis_store_unlock();
        return;
    }
    char* buf = malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        analysis_store_unlock();
        return;
    }
    fread(buf, 1, (size_t)len, f);
    buf[len] = '\0';
    fclose(f);

    json_object* root = json_tokener_parse(buf);
    free(buf);
    if (!root || !json_object_is_type(root, json_type_array)) {
        if (root) json_object_put(root);
        analysis_store_unlock();
        return;
    }

    size_t arrLen = json_object_array_length(root);
    for (size_t i = 0; i < arrLen; ++i) {
        json_object* obj = json_object_array_get_idx(root, i);
        if (!obj) continue;
        json_object* jpath = NULL;
        json_object* jstamp = NULL;
        json_object* jdiags = NULL;
        if (!json_object_object_get_ex(obj, "path", &jpath) ||
            !json_object_object_get_ex(obj, "diagnostics", &jdiags)) {
            continue;
        }
        const char* pathStr = json_object_get_string(jpath);
        size_t dcount = json_object_array_length(jdiags);
        FisicsDiagnostic* tmp = calloc(dcount, sizeof(FisicsDiagnostic));
        if (!tmp) continue;
        for (size_t d = 0; d < dcount; ++d) {
            json_object* jd = json_object_array_get_idx(jdiags, d);
            if (!jd) continue;
            json_object* jline=NULL,* jcol=NULL,* jsev=NULL,* jmsg=NULL,* jcat=NULL,* jcode=NULL;
            json_object_object_get_ex(jd, "line", &jline);
            json_object_object_get_ex(jd, "col", &jcol);
            json_object_object_get_ex(jd, "severity", &jsev);
            json_object_object_get_ex(jd, "category", &jcat);
            json_object_object_get_ex(jd, "code_id", &jcode);
            json_object_object_get_ex(jd, "message", &jmsg);
            tmp[d].file_path = (char*)pathStr;
            tmp[d].line = jline ? json_object_get_int(jline) : 0;
            tmp[d].column = jcol ? json_object_get_int(jcol) : 0;
            int sev = jsev ? json_object_get_int(jsev) : 0;
            tmp[d].kind = (sev == DIAG_SEVERITY_WARNING) ? DIAG_WARNING
                       : (sev == DIAG_SEVERITY_INFO) ? DIAG_NOTE
                       : DIAG_ERROR;
            tmp[d].severity_id = (sev == DIAG_SEVERITY_WARNING) ? FISICS_DIAG_SEVERITY_WARNING
                               : (sev == DIAG_SEVERITY_INFO) ? FISICS_DIAG_SEVERITY_INFO
                               : FISICS_DIAG_SEVERITY_ERROR;
            tmp[d].category_id = map_fisics_category(
                jcat ? (DiagnosticCategory)json_object_get_int(jcat) : DIAG_CATEGORY_UNKNOWN);
            tmp[d].code_id = jcode ? json_object_get_int(jcode) : 0;
            tmp[d].code = tmp[d].code_id;
            if (jmsg) {
                const char* m = json_object_get_string(jmsg);
                tmp[d].message = m ? strdup(m) : NULL;
            }
        }
        analysis_store_upsert_locked(pathStr, tmp, dcount);
        for (size_t d = 0; d < dcount; ++d) {
            free(tmp[d].message);
        }
        free(tmp);
        if (json_object_object_get_ex(obj, "stamp", &jstamp)) {
            long long s = json_object_get_int64(jstamp);
            if (s > 0 && (uint64_t)s > g_stamp_counter) g_stamp_counter = (uint64_t)s;
        }
    }
    json_object_put(root);
    analysis_store_unlock();
}
