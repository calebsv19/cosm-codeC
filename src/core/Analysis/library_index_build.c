#include "core/Analysis/library_index.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fisics_frontend.h"
#include "core/Analysis/analysis_status.h"
#include "core/Analysis/analysis_job.h"
#include "core/Analysis/include_path_resolver.h"
#include "core/Analysis/fisics_frontend_guard.h"
#include "app/GlobalInfo/workspace_prefs.h"

static LibraryIndexBuildStats g_lastBuildStats = {0};
static int g_libraryDebugLogging = 0;
static BuildFlagSet g_buildFlags = {0};
static const BuildFlagSet* g_activeFlags = NULL;

static bool should_suppress_frontend_stderr(void) {
    return !analysis_frontend_logs_enabled();
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

// Simple file read (mirrors project_scan); 32MB cap.
static char* read_file(const char* path, size_t* outLen) {
    if (outLen) *outLen = 0;
    FILE* f = fopen(path, "rb");
    if (!f) {
        g_lastBuildStats.read_failures++;
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 0 || len > (32 * 1024 * 1024)) {
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

static bool is_generated_gperf_source(const char* path) {
    if (!path || !has_ext(path, ".c")) return false;
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    char line[96];
    bool generated = false;
    if (fgets(line, sizeof(line), f)) {
        generated = strncmp(line, "/* C code produced by gperf version", 35) == 0;
    }
    fclose(f);
    return generated;
}

static bool should_analyze_source_path(const char* path) {
    if (!path || !*path) return false;
    if (!(has_ext(path, ".c") || has_ext(path, ".h"))) return false;
    return !is_generated_gperf_source(path);
}

static int should_skip_dir(const char* name) {
    return strcmp(name, ".") == 0 ||
           strcmp(name, "..") == 0 ||
           strcmp(name, "build") == 0 ||
           strcmp(name, "ide_files") == 0 ||
           strcmp(name, ".git") == 0;
}

static bool is_allowed_root_dir(const char* name) {
    if (!name) return false;
    return strcmp(name, "src") == 0 || strcmp(name, "include") == 0;
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

static void scan_dir(const char* root, int depth) {
    if (analysis_job_cancel_requested()) return;
    DIR* dir = opendir(root);
    if (!dir) return;

    struct dirent* ent;
    char child[1024];
    while ((ent = readdir(dir)) != NULL) {
        if (analysis_job_cancel_requested()) break;
        if (should_skip_dir(ent->d_name)) continue;
        if (depth == 0 && !is_allowed_root_dir(ent->d_name)) continue;

        snprintf(child, sizeof(child), "%s/%s", root, ent->d_name);
        struct stat st;
        if (stat(child, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            scan_dir(child, depth + 1);
        } else if (S_ISREG(st.st_mode)) {
            if (!should_analyze_source_path(child)) continue;

            g_lastBuildStats.files_seen++;
            size_t len = 0;
            char* buf = read_file(child, &len);
            if (!buf) continue;

            FisicsAnalysisResult res;
            memset(&res, 0, sizeof(res));
            FisicsFrontendOptions opts = {0};
            const BuildFlagSet* flags = g_activeFlags ? g_activeFlags : &g_buildFlags;
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
            bool ok = fisics_analyze_buffer(child, buf, len, &opts, &res);
            if (saved_stderr >= 0) {
                suppress_stderr_end(saved_stderr);
            }
            if (saved_stdout >= 0) {
                suppress_stdout_end(saved_stdout);
            }
            fisics_frontend_guard_unlock();
            if (!ok) g_lastBuildStats.analysis_failures++;
            g_lastBuildStats.files_analyzed++;

            if (analysis_job_cancel_requested()) {
                fisics_free_analysis_result(&res);
                free(buf);
                continue;
            }

            if (g_libraryDebugLogging) {
                printf("[LibraryIndex] scan %s (includes: %zu, ok: %d)\n",
                       child, res.include_count, ok ? 1 : 0);
            }

            for (size_t i = 0; i < res.include_count; ++i) {
                const FisicsInclude* inc = &res.includes[i];
                char fallbackName[512];
                const char* includeName = include_label_or_fallback(inc, fallbackName, sizeof(fallbackName));
                if (!includeName) continue;
                g_lastBuildStats.includes_added++;
                library_index_add_include(child,
                                          includeName,
                                          inc->resolved_path,
                                          map_kind(inc->kind),
                                          map_origin(inc->origin),
                                          inc->line,
                                          inc->column);
            }

            fisics_free_analysis_result(&res);
            free(buf);
            analysis_job_maybe_throttle();
        }
    }
    closedir(dir);
}

void library_index_build_workspace(const char* project_root) {
    memset(&g_lastBuildStats, 0, sizeof(g_lastBuildStats));
    g_libraryDebugLogging = 0;
    const char* debugEnv = getenv("LIBRARY_INDEX_DEBUG");
    if (debugEnv && *debugEnv && debugEnv[0] != '0') {
        g_libraryDebugLogging = 1;
    }

    library_index_begin(project_root);
    const WorkspaceBuildConfig* cfg = getWorkspaceBuildConfig();
    const char* flags = (cfg && cfg->build_args[0]) ? cfg->build_args : NULL;
    gather_build_flags(project_root, flags, &g_buildFlags);
    g_activeFlags = &g_buildFlags;
    if (!project_root || !*project_root) {
        library_index_finalize();
        g_activeFlags = NULL;
        free_build_flag_set(&g_buildFlags);
        g_buildFlags = (BuildFlagSet){0};
        return;
    }
    scan_dir(project_root, 0);
    library_index_finalize();
    g_activeFlags = NULL;
    free_build_flag_set(&g_buildFlags);
    g_buildFlags = (BuildFlagSet){0};

    if (g_libraryDebugLogging) {
        printf("[LibraryIndex] build complete root=%s files:%zu analyzed:%zu includes:%zu read_fail:%zu analysis_fail:%zu\n",
               project_root ? project_root : "(null)",
               g_lastBuildStats.files_seen,
               g_lastBuildStats.files_analyzed,
               g_lastBuildStats.includes_added,
               g_lastBuildStats.read_failures,
               g_lastBuildStats.analysis_failures);
    }
}

void library_index_get_last_build_stats(LibraryIndexBuildStats* outStats) {
    if (!outStats) return;
    *outStats = g_lastBuildStats;
}

void library_index_build_workspace_with_flags(const char* project_root, const BuildFlagSet* flags) {
    if (!flags) return;
    memset(&g_lastBuildStats, 0, sizeof(g_lastBuildStats));
    g_libraryDebugLogging = 0;
    const char* debugEnv = getenv("LIBRARY_INDEX_DEBUG");
    if (debugEnv && *debugEnv && debugEnv[0] != '0') {
        g_libraryDebugLogging = 1;
    }

    library_index_begin(project_root);
    g_activeFlags = flags;
    if (project_root && *project_root) {
        scan_dir(project_root, 0);
    }
    library_index_finalize();
    g_activeFlags = NULL;
}
