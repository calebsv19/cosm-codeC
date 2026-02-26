#include "core/Analysis/project_scan.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fisics_frontend.h"
#include "core/Analysis/analysis_store.h"
#include "core/Analysis/analysis_symbols_store.h"
#include "core/Analysis/analysis_token_store.h"
#include "core/Analysis/analysis_status.h"
#include "core/Analysis/include_graph.h"
#include "core/Analysis/include_path_resolver.h"
#include "core/Analysis/library_index.h"
#include "core/Analysis/fisics_frontend_guard.h"
#include "core/Analysis/analysis_job.h"
#include "app/GlobalInfo/workspace_prefs.h"

static char* read_file(const char* path, size_t* outLen) {
    if (outLen) *outLen = 0;
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 0 || len > (32 * 1024 * 1024)) { // 32MB cap
        fclose(f);
        return NULL;
    }
    char* buf = malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t n = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[n] = '\0';
    if (outLen) *outLen = n;
    return buf;
}

static int has_ext(const char* name, const char* ext) {
    size_t ln = strlen(name);
    size_t le = strlen(ext);
    if (ln < le) return 0;
    return strcasecmp(name + ln - le, ext) == 0;
}

static int should_skip_dir(const char* name) {
    return strcmp(name, ".") == 0 ||
           strcmp(name, "..") == 0 ||
           strcmp(name, "build") == 0 ||
           strcmp(name, "ide_files") == 0 ||
           strcmp(name, ".git") == 0 ||
           strcmp(name, ".DS_Store") == 0;
}

static BuildFlagSet g_buildFlags = {0};
static const BuildFlagSet* g_activeFlags = NULL;
static const char* g_activeWorkspaceRoot = NULL;
static bool g_update_library_index = false;
static int g_analysis_progress_total = 0;
static int g_analysis_progress_done = 0;

static bool should_suppress_frontend_stderr(void) {
    return !analysis_frontend_logs_enabled();
}

static bool should_print_file_progress(void) {
    const char* env = getenv("IDE_ANALYSIS_FILE_PROGRESS");
    // Default is off. Set IDE_ANALYSIS_FILE_PROGRESS=1 to enable per-file progress logs.
    return (env && env[0] == '1');
}

static int suppress_stderr_begin(void) {
    int saved = dup(STDERR_FILENO);
    if (saved < 0) return -1;
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull < 0) {
        close(saved);
        return -1;
    }
    if (dup2(devnull, STDERR_FILENO) < 0) {
        close(devnull);
        close(saved);
        return -1;
    }
    close(devnull);
    return saved;
}

static void suppress_stderr_end(int saved_fd) {
    if (saved_fd < 0) return;
    (void)dup2(saved_fd, STDERR_FILENO);
    close(saved_fd);
}

static int suppress_stdout_begin(void) {
    int saved = dup(STDOUT_FILENO);
    if (saved < 0) return -1;
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull < 0) {
        close(saved);
        return -1;
    }
    if (dup2(devnull, STDOUT_FILENO) < 0) {
        close(devnull);
        close(saved);
        return -1;
    }
    close(devnull);
    return saved;
}

static void suppress_stdout_end(int saved_fd) {
    if (saved_fd < 0) return;
    (void)dup2(saved_fd, STDOUT_FILENO);
    close(saved_fd);
}

static LibraryBucketKind map_origin(FisicsIncludeOrigin origin) {
    switch (origin) {
        case FISICS_INCLUDE_PROJECT:        return LIB_BUCKET_PROJECT;
        case FISICS_INCLUDE_SYSTEM_ORIGIN:  return LIB_BUCKET_SYSTEM;
        case FISICS_INCLUDE_EXTERNAL:       return LIB_BUCKET_EXTERNAL;
        case FISICS_INCLUDE_UNRESOLVED:
        default:                            return LIB_BUCKET_UNRESOLVED;
    }
}

static LibraryIncludeKind map_kind(FisicsIncludeKind kind) {
    return (kind == FISICS_INCLUDE_SYSTEM) ? LIB_INCLUDE_KIND_SYSTEM
                                           : LIB_INCLUDE_KIND_LOCAL;
}

static const char* include_label_or_fallback(const FisicsInclude* inc,
                                             char* fallback,
                                             size_t fallback_size) {
    if (!inc) return NULL;
    if (inc->name && inc->name[0] != '\0') return inc->name;
    if (!fallback || fallback_size == 0) return NULL;
    fallback[0] = '\0';
    if (!inc->resolved_path || inc->resolved_path[0] == '\0') return NULL;

    const char* base = strrchr(inc->resolved_path, '/');
    const char* bslash = strrchr(inc->resolved_path, '\\');
    if (!base || (bslash && bslash > base)) base = bslash;
    base = base ? (base + 1) : inc->resolved_path;
    if (!base || base[0] == '\0') return NULL;

    snprintf(fallback, fallback_size, "%s", base);
    return fallback[0] ? fallback : NULL;
}

static bool is_allowed_root_dir(const char* name) {
    if (!name) return false;
    return strcmp(name, "src") == 0 || strcmp(name, "include") == 0;
}

typedef struct {
    char* path;
    int depth;
} DirQueueEntry;

static int count_scannable_files_in_dir(const char* root) {
    if (!root || !*root) return 0;

    size_t count = 0;
    size_t capacity = 32;
    int total = 0;
    DirQueueEntry* stack = (DirQueueEntry*)malloc(capacity * sizeof(DirQueueEntry));
    if (!stack) return 0;

    stack[count++] = (DirQueueEntry){ strdup(root), 0 };

    while (count > 0) {
        DirQueueEntry cur = stack[--count];
        if (!cur.path) continue;

        DIR* dir = opendir(cur.path);
        if (!dir) {
            free(cur.path);
            continue;
        }

        struct dirent* ent;
        char child[1024];
        while ((ent = readdir(dir)) != NULL) {
            if (analysis_job_cancel_requested()) break;
            if (should_skip_dir(ent->d_name)) continue;
            if (cur.depth == 0 && !is_allowed_root_dir(ent->d_name)) continue;

            snprintf(child, sizeof(child), "%s/%s", cur.path, ent->d_name);
            struct stat st;
            if (stat(child, &st) != 0) continue;

            if (S_ISDIR(st.st_mode)) {
                if (count >= capacity) {
                    size_t newCap = capacity * 2;
                    DirQueueEntry* grown = (DirQueueEntry*)realloc(stack, newCap * sizeof(DirQueueEntry));
                    if (!grown) continue;
                    stack = grown;
                    capacity = newCap;
                }
                stack[count++] = (DirQueueEntry){ strdup(child), cur.depth + 1 };
            } else if (S_ISREG(st.st_mode)) {
                if (has_ext(ent->d_name, ".c") || has_ext(ent->d_name, ".h")) {
                    total++;
                }
            }
        }
        closedir(dir);
        free(cur.path);
    }

    free(stack);
    return total;
}

static int count_scannable_files_in_list(const char* const* files, size_t file_count) {
    if (!files || file_count == 0) return 0;
    int total = 0;
    for (size_t i = 0; i < file_count; ++i) {
        if (analysis_job_cancel_requested()) break;
        const char* path = files[i];
        if (!path || !*path) continue;

        struct stat st;
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) continue;
        if (!(has_ext(path, ".c") || has_ext(path, ".h"))) continue;
        total++;
    }
    return total;
}

static void analyze_file_with_active_flags(const char* file_path) {
    if (!file_path || !*file_path) return;
    if (analysis_job_cancel_requested()) return;

    size_t len = 0;
    char* buf = read_file(file_path, &len);
    if (!buf) return;

    FisicsAnalysisResult res;
    memset(&res, 0, sizeof(res));

    const BuildFlagSet* flags = g_activeFlags ? g_activeFlags : &g_buildFlags;
    FisicsFrontendOptions opts = {0};
    opts.include_paths = (const char* const*)flags->include_paths;
    opts.include_path_count = flags->include_count;
    opts.macro_defines = (const char* const*)flags->macro_defines;
    opts.macro_define_count = flags->macro_count;

    int saved_stderr = -1;
    int saved_stdout = -1;
    fisics_frontend_guard_lock();
    if (should_suppress_frontend_stderr()) {
        saved_stdout = suppress_stdout_begin();
        saved_stderr = suppress_stderr_begin();
    }
    bool ok = fisics_analyze_buffer(file_path, buf, len, &opts, &res);
    if (saved_stderr >= 0) {
        suppress_stderr_end(saved_stderr);
    }
    if (saved_stdout >= 0) {
        suppress_stdout_end(saved_stdout);
    }
    fisics_frontend_guard_unlock();
    (void)ok;

    if (analysis_job_cancel_requested()) {
        fisics_free_analysis_result(&res);
        free(buf);
        return;
    }

    analysis_store_upsert(file_path, res.diagnostics, res.diag_count);
    analysis_symbols_store_upsert(file_path, res.symbols, res.symbol_count);
    analysis_token_store_upsert(file_path, res.tokens, res.token_count);
    include_graph_replace_from_result(file_path, res.includes, res.include_count, g_activeWorkspaceRoot);
    if (g_update_library_index) {
        library_index_remove_source(file_path);
        for (size_t i = 0; i < res.include_count; ++i) {
            const FisicsInclude* inc = &res.includes[i];
            char fallbackName[512];
            const char* includeName = include_label_or_fallback(inc, fallbackName, sizeof(fallbackName));
            if (!includeName) continue;
            library_index_add_include(file_path,
                                      includeName,
                                      inc->resolved_path,
                                      map_kind(inc->kind),
                                      map_origin(inc->origin),
                                      inc->line,
                                      inc->column);
        }
    }

    fisics_free_analysis_result(&res);
    free(buf);

    g_analysis_progress_done++;
    analysis_status_set_progress(g_analysis_progress_done, g_analysis_progress_total);
    if (should_print_file_progress()) {
        printf("[Analysis] [%d] %s : %s (diag:%zu sym:%zu inc:%zu)\n",
               g_analysis_progress_done,
               file_path,
               ok ? "ok" : "failed",
               res.diag_count,
               res.symbol_count,
               res.include_count);
    }
    analysis_job_maybe_throttle();
}

static void scan_dir(const char* root) {
    if (!root || !*root) return;

    size_t count = 0;
    size_t capacity = 32;
    DirQueueEntry* stack = (DirQueueEntry*)malloc(capacity * sizeof(DirQueueEntry));
    if (!stack) return;

    stack[count++] = (DirQueueEntry){ strdup(root), 0 };

    while (count > 0) {
        if (analysis_job_cancel_requested()) break;
        DirQueueEntry cur = stack[--count];
        if (!cur.path) continue;

        DIR* dir = opendir(cur.path);
        if (!dir) {
            free(cur.path);
            continue;
        }

        struct dirent* ent;
        char child[1024];
        while ((ent = readdir(dir)) != NULL) {
            if (analysis_job_cancel_requested()) break;
            if (should_skip_dir(ent->d_name)) continue;
            if (cur.depth == 0 && !is_allowed_root_dir(ent->d_name)) {
                continue;
            }

            snprintf(child, sizeof(child), "%s/%s", cur.path, ent->d_name);
            struct stat st;
            if (stat(child, &st) != 0) continue;

            if (S_ISDIR(st.st_mode)) {
                if (count >= capacity) {
                    size_t newCap = capacity * 2;
                    DirQueueEntry* grown = (DirQueueEntry*)realloc(stack, newCap * sizeof(DirQueueEntry));
                    if (!grown) {
                        continue;
                    }
                    stack = grown;
                    capacity = newCap;
                }
                stack[count++] = (DirQueueEntry){ strdup(child), cur.depth + 1 };
            } else if (S_ISREG(st.st_mode)) {
                if (!(has_ext(ent->d_name, ".c") || has_ext(ent->d_name, ".h"))) continue;
                analyze_file_with_active_flags(child);
            }
        }
        closedir(dir);
        free(cur.path);
    }
    free(stack);
}

void analysis_scan_workspace(const char* root) {
    if (!root || !*root) return;
    analysis_store_clear();
    g_analysis_progress_total = count_scannable_files_in_dir(root);
    g_analysis_progress_done = 0;
    analysis_status_set_progress(0, g_analysis_progress_total);
    const WorkspaceBuildConfig* cfg = getWorkspaceBuildConfig();
    const char* flags = (cfg && cfg->build_args[0]) ? cfg->build_args : NULL;
    gather_build_flags(root, flags, &g_buildFlags);
    g_activeFlags = &g_buildFlags;
    scan_dir(root);
    g_activeFlags = NULL;
    free_build_flag_set(&g_buildFlags);
    analysis_store_flatten_to_engine();
    analysis_store_save(root);
    analysis_symbols_store_save(root);
    analysis_token_store_save(root);
}

void analysis_scan_workspace_with_flags(const char* root, const BuildFlagSet* flags, bool update_engine) {
    if (!root || !*root || !flags) return;
    analysis_store_clear();
    include_graph_clear();
    g_analysis_progress_total = count_scannable_files_in_dir(root);
    g_analysis_progress_done = 0;
    analysis_status_set_progress(0, g_analysis_progress_total);
    g_activeFlags = flags;
    g_activeWorkspaceRoot = root;
    g_update_library_index = false;
    scan_dir(root);
    g_activeFlags = NULL;
    g_activeWorkspaceRoot = NULL;
    g_update_library_index = false;
    if (update_engine) {
        analysis_store_flatten_to_engine();
    }
    analysis_store_save(root);
    analysis_symbols_store_save(root);
    analysis_token_store_save(root);
    include_graph_save(root);
}

void analysis_scan_files_with_flags(const char* root,
                                    const char* const* files,
                                    size_t file_count,
                                    const BuildFlagSet* flags,
                                    bool update_engine,
                                    bool persist_outputs) {
    if (!root || !*root || !files || file_count == 0 || !flags) return;
    g_activeFlags = flags;
    g_activeWorkspaceRoot = root;
    g_update_library_index = true;
    g_analysis_progress_total = count_scannable_files_in_list(files, file_count);
    g_analysis_progress_done = 0;
    analysis_status_set_progress(0, g_analysis_progress_total);

    for (size_t i = 0; i < file_count; ++i) {
        if (analysis_job_cancel_requested()) break;
        const char* path = files[i];
        if (!path || !*path) continue;

        struct stat st;
        if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) {
            analysis_store_remove(path);
            analysis_symbols_store_remove(path);
            analysis_token_store_remove(path);
            include_graph_remove_source(path);
            library_index_remove_source(path);
            continue;
        }
        if (!(has_ext(path, ".c") || has_ext(path, ".h"))) continue;
        analyze_file_with_active_flags(path);
    }

    g_activeFlags = NULL;
    g_activeWorkspaceRoot = NULL;
    g_update_library_index = false;
    if (update_engine) {
        analysis_store_flatten_to_engine();
    }
    if (persist_outputs) {
        analysis_store_save(root);
        analysis_symbols_store_save(root);
        analysis_token_store_save(root);
        include_graph_save(root);
        library_index_save(root);
    }
}
