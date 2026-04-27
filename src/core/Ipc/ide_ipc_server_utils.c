#include "core/Ipc/ide_ipc_server_utils.h"

#include "core/Ipc/ide_ipc_path_guard.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define IDE_IPC_APP_DIR "caleb_ide"
#define IDE_IPC_MAX_PATCH_FILES 128

static long long now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000LL + (long long)(tv.tv_usec / 1000);
}

static void str_copy(char* dst, size_t cap, const char* src) {
    if (!dst || cap == 0) return;
    if (!src) src = "";
    snprintf(dst, cap, "%s", src);
}

static bool ensure_dir_exists(const char* path, mode_t mode) {
    if (!path || !*path) return false;
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    if (mkdir(path, mode) == 0) return true;
    return errno == EEXIST;
}

bool ide_ipc_generate_auth_token_hex(char out[65]) {
    if (!out) return false;
    unsigned char buf[32];

    int fd = open("/dev/urandom", O_RDONLY);
    if (fd < 0) {
        out[0] = '\0';
        return false;
    }

    size_t off = 0;
    while (off < sizeof(buf)) {
        ssize_t n = read(fd, buf + off, sizeof(buf) - off);
        if (n > 0) {
            off += (size_t)n;
            continue;
        }
        if (n < 0 && errno == EINTR) continue;
        break;
    }
    close(fd);
    if (off != sizeof(buf)) {
        out[0] = '\0';
        return false;
    }

    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < sizeof(buf); ++i) {
        out[i * 2] = hex[(buf[i] >> 4) & 0x0F];
        out[i * 2 + 1] = hex[buf[i] & 0x0F];
    }
    out[64] = '\0';
    return true;
}

bool ide_ipc_build_socket_path(char* out, size_t out_cap, char* out_session, size_t session_cap) {
    if (!out || out_cap == 0 || !out_session || session_cap == 0) return false;

    const char* home = getenv("HOME");
    const char* xdg_cache = getenv("XDG_CACHE_HOME");
    long long t = now_ms();
    pid_t pid = getpid();
    snprintf(out_session, session_cap, "pid_%d_%lld", (int)pid, t);

    char sock_dir[1024];
    bool have_sock_dir = false;
    if (xdg_cache && *xdg_cache) {
        char app_dir[1024];
        snprintf(app_dir, sizeof(app_dir), "%s/%s", xdg_cache, IDE_IPC_APP_DIR);
        snprintf(sock_dir, sizeof(sock_dir), "%s/sock", app_dir);
        if (ensure_dir_exists(xdg_cache, 0700) &&
            ensure_dir_exists(app_dir, 0700) &&
            ensure_dir_exists(sock_dir, 0700)) {
            have_sock_dir = true;
        }
    } else if (home && *home) {
        char cache_dir[1024];
        char app_dir[1024];
        snprintf(cache_dir, sizeof(cache_dir), "%s/.cache", home);
        snprintf(app_dir, sizeof(app_dir), "%s/%s", cache_dir, IDE_IPC_APP_DIR);
        snprintf(sock_dir, sizeof(sock_dir), "%s/sock", app_dir);
        if (ensure_dir_exists(cache_dir, 0700) &&
            ensure_dir_exists(app_dir, 0700) &&
            ensure_dir_exists(sock_dir, 0700)) {
            have_sock_dir = true;
        }
    }
    if (!have_sock_dir) {
        snprintf(sock_dir, sizeof(sock_dir), "/tmp/%s_sock", IDE_IPC_APP_DIR);
        if (!ensure_dir_exists(sock_dir, 0700)) return false;
    }

    char candidate[256];
    snprintf(candidate, sizeof(candidate), "%s/%s.sock", sock_dir, out_session);

    if (strlen(candidate) >= out_cap) {
        snprintf(candidate, sizeof(candidate), "/tmp/%s_%d.sock", IDE_IPC_APP_DIR, (int)pid);
        if (strlen(candidate) >= out_cap) return false;
    }

    snprintf(out, out_cap, "%s", candidate);
    return true;
}

static bool compute_file_hash_hex_local(const char* path, char* out_hex, size_t out_hex_cap) {
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
    snprintf(out_hex, out_hex_cap, "%016llx", hash);
    return true;
}

static int collect_diff_target_paths(const char* diff_text, char paths[][1024], int max_paths) {
    if (!diff_text || !*diff_text || !paths || max_paths <= 0) return 0;
    int count = 0;
    char* tmp = strdup(diff_text);
    if (!tmp) return 0;
    char* save = NULL;
    for (char* line = strtok_r(tmp, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        if (strncmp(line, "+++ ", 4) != 0) continue;
        const char* p = line + 4;
        while (*p == ' ' || *p == '\t') p++;
        if (strncmp(p, "a/", 2) == 0 || strncmp(p, "b/", 2) == 0) p += 2;
        if (strncmp(p, "/dev/null", 9) == 0) continue;
        char path[1024];
        size_t i = 0;
        while (p[i] && p[i] != '\t' && p[i] != ' ' && i < sizeof(path) - 1) {
            path[i] = p[i];
            i++;
        }
        path[i] = '\0';
        if (!path[0]) continue;
        bool exists = false;
        for (int k = 0; k < count; ++k) {
            if (strcmp(paths[k], path) == 0) {
                exists = true;
                break;
            }
        }
        if (!exists && count < max_paths) {
            snprintf(paths[count], 1024, "%s", path);
            count++;
        }
    }
    free(tmp);
    return count;
}

bool ide_ipc_verify_edit_hashes(const char* project_root,
                                const char* diff_text,
                                bool check_hash,
                                json_object* hashes_obj,
                                char* error_out,
                                size_t error_cap) {
    if (!check_hash) return true;
    if (!hashes_obj || !json_object_is_type(hashes_obj, json_type_object)) {
        str_copy(error_out, error_cap, "Hash check is enabled but args.hashes object is missing");
        return false;
    }

    char paths[IDE_IPC_MAX_PATCH_FILES][1024];
    int path_count = collect_diff_target_paths(diff_text, paths, IDE_IPC_MAX_PATCH_FILES);
    if (path_count <= 0) {
        str_copy(error_out, error_cap, "No target files found in diff");
        return false;
    }

    for (int i = 0; i < path_count; ++i) {
        const char* rel = paths[i];
        char abs[1024] = {0};
        if (!ide_ipc_resolve_workspace_existing_path(project_root,
                                                     rel,
                                                     abs,
                                                     sizeof(abs),
                                                     error_out,
                                                     error_cap)) {
            return false;
        }

        json_object* expected_obj = NULL;
        const char* expected = NULL;
        if (json_object_object_get_ex(hashes_obj, rel, &expected_obj) &&
            expected_obj && json_object_is_type(expected_obj, json_type_string)) {
            expected = json_object_get_string(expected_obj);
        } else if (json_object_object_get_ex(hashes_obj, abs, &expected_obj) &&
                   expected_obj && json_object_is_type(expected_obj, json_type_string)) {
            expected = json_object_get_string(expected_obj);
        }

        if (!expected || !*expected) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Missing expected hash for %s", rel);
            str_copy(error_out, error_cap, msg);
            return false;
        }

        char actual[32];
        if (!compute_file_hash_hex_local(abs, actual, sizeof(actual))) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Failed to hash target file %s", rel);
            str_copy(error_out, error_cap, msg);
            return false;
        }

        if (strcmp(actual, expected) != 0) {
            char msg[320];
            snprintf(msg, sizeof(msg), "Hash mismatch for %s (expected %s got %s)", rel, expected, actual);
            str_copy(error_out, error_cap, msg);
            return false;
        }
    }

    return true;
}

json_object* ide_ipc_build_error_obj(const char* code, const char* message, const char* details) {
    json_object* err = json_object_new_object();
    json_object_object_add(err, "code", json_object_new_string(code ? code : "error"));
    json_object_object_add(err, "message", json_object_new_string(message ? message : "Unknown error"));
    if (details && *details) {
        json_object_object_add(err, "details", json_object_new_string(details));
    }
    return err;
}

char* ide_ipc_build_response_json(const char* id, bool ok, json_object* result, json_object* error) {
    json_object* root = json_object_new_object();
    json_object_object_add(root, "id", json_object_new_string(id && *id ? id : "unknown"));
    json_object_object_add(root, "ok", json_object_new_boolean(ok));
    if (ok) {
        if (!result) result = json_object_new_object();
        json_object_object_add(root, "result", result);
    } else {
        if (!error) error = ide_ipc_build_error_obj("internal_error", "Missing error payload", NULL);
        json_object_object_add(root, "error", error);
    }

    const char* text = json_object_to_json_string_ext(root, JSON_C_TO_STRING_PLAIN);
    size_t len = strlen(text);
    char* out = (char*)malloc(len + 2);
    if (!out) {
        json_object_put(root);
        return NULL;
    }
    memcpy(out, text, len);
    out[len] = '\n';
    out[len + 1] = '\0';
    json_object_put(root);
    return out;
}
