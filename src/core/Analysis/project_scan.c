#include "core/Analysis/project_scan.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "fisics_frontend.h"
#include "core/Analysis/analysis_store.h"

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
           strcmp(name, ".git") == 0;
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

            size_t len = 0;
            char* buf = read_file(child, &len);
            if (!buf) continue;

            FisicsAnalysisResult res;
            memset(&res, 0, sizeof(res));

            // Run analysis on the in-memory buffer so we get diagnostics without
            // requiring a separate disk read inside the frontend.
            bool ok = fisics_analyze_buffer(child, buf, len, &res);
            (void)ok; // Even if false, the result may still contain diagnostics.

            // Upsert per-file diagnostics; store will copy strings and manage lifetime.
            analysis_store_upsert(child, res.diagnostics, res.diag_count);

            fisics_free_analysis_result(&res);
            free(buf);
        }
    }
    closedir(dir);
}

void analysis_scan_workspace(const char* root) {
    if (!root || !*root) return;
    analysis_store_clear();
    scan_dir(root);
    analysis_store_flatten_to_engine();
    analysis_store_save(root);
}
