#include "core/Analysis/library_index.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "fisics_frontend.h"
#include "core/Analysis/include_path_resolver.h"
#include "app/GlobalInfo/workspace_prefs.h"

static LibraryIndexBuildStats g_lastBuildStats = {0};
static int g_libraryDebugLogging = 0;
static BuildFlagSet g_buildFlags = {0};
static const BuildFlagSet* g_activeFlags = NULL;

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

static int should_skip_dir(const char* name) {
    return strcmp(name, ".") == 0 ||
           strcmp(name, "..") == 0 ||
           strcmp(name, "build") == 0 ||
           strcmp(name, "ide_files") == 0 ||
           strcmp(name, ".git") == 0;
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

static void scan_dir(const char* root) {
    DIR* dir = opendir(root);
    if (!dir) return;

    struct dirent* ent;
    char child[1024];
    while ((ent = readdir(dir)) != NULL) {
        if (should_skip_dir(ent->d_name)) continue;

        snprintf(child, sizeof(child), "%s/%s", root, ent->d_name);
        struct stat st;
        if (stat(child, &st) != 0) continue;

    if (S_ISDIR(st.st_mode)) {
            scan_dir(child);
        } else if (S_ISREG(st.st_mode)) {
            if (!(has_ext(ent->d_name, ".c") || has_ext(ent->d_name, ".h"))) continue;

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

            bool ok = fisics_analyze_buffer(child, buf, len, &opts, &res);
            if (!ok) g_lastBuildStats.analysis_failures++;
            g_lastBuildStats.files_analyzed++;

            if (g_libraryDebugLogging) {
                printf("[LibraryIndex] scan %s (includes: %zu, ok: %d)\n",
                       child, res.include_count, ok ? 1 : 0);
            }

            for (size_t i = 0; i < res.include_count; ++i) {
                const FisicsInclude* inc = &res.includes[i];
                if (!inc || !inc->name) continue;
                g_lastBuildStats.includes_added++;
                library_index_add_include(child,
                                          inc->name,
                                          inc->resolved_path,
                                          map_kind(inc->kind),
                                          map_origin(inc->origin),
                                          inc->line,
                                          inc->column);
            }

            fisics_free_analysis_result(&res);
            free(buf);
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
    scan_dir(project_root);
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
        scan_dir(project_root);
    }
    library_index_finalize();
    g_activeFlags = NULL;
}
