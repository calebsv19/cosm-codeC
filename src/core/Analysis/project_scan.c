#include "core/Analysis/project_scan.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "fisics_frontend.h"
#include "core/Analysis/analysis_store.h"
#include "core/Analysis/analysis_symbols_store.h"
#include "core/Analysis/analysis_token_store.h"
#include "core/Analysis/include_path_resolver.h"
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

static bool is_allowed_root_dir(const char* name) {
    if (!name) return false;
    return strcmp(name, "src") == 0 || strcmp(name, "include") == 0;
}

typedef struct {
    char* path;
    int depth;
} DirQueueEntry;

static void scan_dir(const char* root) {
    if (!root || !*root) return;

    size_t count = 0;
    size_t capacity = 32;
    DirQueueEntry* stack = (DirQueueEntry*)malloc(capacity * sizeof(DirQueueEntry));
    if (!stack) return;

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

                size_t len = 0;
                char* buf = read_file(child, &len);
                if (!buf) continue;

                FisicsAnalysisResult res;
                memset(&res, 0, sizeof(res));

                // Run analysis on the in-memory buffer so we get diagnostics without
                // requiring a separate disk read inside the frontend.
                const BuildFlagSet* flags = g_activeFlags ? g_activeFlags : &g_buildFlags;
                FisicsFrontendOptions opts = {0};
                opts.include_paths = (const char* const*)flags->include_paths;
                opts.include_path_count = flags->include_count;
                opts.macro_defines = (const char* const*)flags->macro_defines;
                opts.macro_define_count = flags->macro_count;

                bool ok = fisics_analyze_buffer(child, buf, len, &opts, &res);
                (void)ok; // Even if false, the result may still contain diagnostics.

                // Upsert per-file diagnostics; store will copy strings and manage lifetime.
                analysis_store_upsert(child, res.diagnostics, res.diag_count);
                analysis_symbols_store_upsert(child, res.symbols, res.symbol_count);
                analysis_token_store_upsert(child, res.tokens, res.token_count);

                fisics_free_analysis_result(&res);
                free(buf);
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
    g_activeFlags = flags;
    scan_dir(root);
    g_activeFlags = NULL;
    if (update_engine) {
        analysis_store_flatten_to_engine();
    }
    analysis_store_save(root);
    analysis_symbols_store_save(root);
    analysis_token_store_save(root);
}
