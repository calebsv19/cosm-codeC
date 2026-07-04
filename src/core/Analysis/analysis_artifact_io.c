#include "core/Analysis/analysis_artifact_io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

bool analysis_artifact_io_ide_dir(const char* workspace_root,
                                  char* out_path,
                                  size_t out_path_size) {
    if (!workspace_root || !*workspace_root || !out_path || out_path_size == 0) {
        return false;
    }
    int n = snprintf(out_path, out_path_size, "%s/ide_files", workspace_root);
    if (n <= 0 || (size_t)n >= out_path_size) {
        out_path[0] = '\0';
        return false;
    }
    return true;
}

bool analysis_artifact_io_path(const char* workspace_root,
                               const char* artifact_name,
                               char* out_path,
                               size_t out_path_size) {
    if (!workspace_root || !*workspace_root || !artifact_name || !*artifact_name ||
        !out_path || out_path_size == 0) {
        return false;
    }
    int n = snprintf(out_path, out_path_size, "%s/ide_files/%s", workspace_root, artifact_name);
    if (n <= 0 || (size_t)n >= out_path_size) {
        out_path[0] = '\0';
        return false;
    }
    return true;
}

bool analysis_artifact_io_ensure_dir(const char* workspace_root) {
    char dir[1024];
    if (!analysis_artifact_io_ide_dir(workspace_root, dir, sizeof(dir))) {
        return false;
    }
    struct stat st;
    if (stat(dir, &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    }
    return mkdir(dir, 0755) == 0;
}

char* analysis_artifact_io_read_text(const char* workspace_root,
                                     const char* artifact_name,
                                     size_t max_bytes,
                                     long* out_len) {
    char path[1024];
    if (out_len) *out_len = 0;
    if (!analysis_artifact_io_path(workspace_root, artifact_name, path, sizeof(path))) {
        return NULL;
    }

    FILE* f = fopen(path, "r");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long len = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return NULL;
    }
    if (len <= 0 || (max_bytes > 0 && (unsigned long)len > (unsigned long)max_bytes)) {
        fclose(f);
        return NULL;
    }

    char* buf = (char*)malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t read_len = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[read_len] = '\0';
    if (out_len) *out_len = (long)read_len;
    return buf;
}

bool analysis_artifact_io_write_text(const char* workspace_root,
                                     const char* artifact_name,
                                     const char* text) {
    char path[1024];
    if (!text || !analysis_artifact_io_ensure_dir(workspace_root) ||
        !analysis_artifact_io_path(workspace_root, artifact_name, path, sizeof(path))) {
        return false;
    }
    FILE* f = fopen(path, "w");
    if (!f) return false;
    bool ok = fputs(text, f) >= 0;
    if (fclose(f) != 0) {
        ok = false;
    }
    return ok;
}

bool analysis_artifact_io_write_text_atomic(const char* workspace_root,
                                            const char* artifact_name,
                                            const char* text) {
    char path[1024];
    char tmp_path[1088];
    if (!text || !analysis_artifact_io_ensure_dir(workspace_root) ||
        !analysis_artifact_io_path(workspace_root, artifact_name, path, sizeof(path))) {
        return false;
    }
    int n = snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    if (n <= 0 || (size_t)n >= sizeof(tmp_path)) {
        return false;
    }

    FILE* f = fopen(tmp_path, "w");
    if (!f) return false;
    bool ok = fputs(text, f) >= 0;
    if (fclose(f) != 0) {
        ok = false;
    }
    if (!ok) {
        unlink(tmp_path);
        return false;
    }
    if (rename(tmp_path, path) != 0) {
        unlink(tmp_path);
        return false;
    }
    return true;
}
