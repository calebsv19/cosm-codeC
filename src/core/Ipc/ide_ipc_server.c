#include "ide_ipc_server.h"
#include "core/BuildSystem/build_diagnostics.h"
#include "app/GlobalInfo/workspace_prefs.h"
#include "core/Ipc/ide_ipc_build_helpers.h"
#include "core/Ipc/ide_ipc_edit_apply.h"
#include "core/Ipc/ide_ipc_path_guard.h"
#include "core/Ipc/ide_ipc_query_helpers.h"
#include "core/Ipc/ide_ipc_search_helpers.h"
#include "core/Ipc/ide_ipc_server_utils.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <json-c/json.h>
#include <limits.h>
#include <pthread.h>
#include <regex.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define IDE_IPC_SERVER_VERSION "0.1.0"
#define IDE_IPC_APP_DIR "caleb_ide"
#define IDE_IPC_MAX_REQUEST_BYTES (256 * 1024)
#define IDE_IPC_MAX_PATCH_FILES 128
#define IDE_IPC_SECURITY_LOG_INTERVAL_MS 10000

typedef struct {
    int listen_fd;
    bool running;
    pthread_t accept_thread;
    pthread_mutex_t lock;

    char socket_path[108];
    char project_root[1024];
    char session_id[128];
    char auth_token[65];

    long long started_at_ms;
    long long auth_log_last_ms;
    unsigned auth_log_suppressed;
    long long peer_reject_log_last_ms;
    unsigned peer_reject_log_suppressed;

    pthread_mutex_t open_lock;
    pthread_cond_t open_cond;
    bool open_inflight;
    bool open_pending;
    bool open_completed;
    bool open_success;
    int open_line;
    int open_col;
    char open_path[1024];
    char open_error[256];
    IdeIpcOpenHandler open_handler;
    void* open_handler_userdata;

    pthread_mutex_t edit_lock;
    pthread_cond_t edit_cond;
    bool edit_inflight;
    bool edit_pending;
    bool edit_completed;
    bool edit_success;
    char* edit_diff;
    char* edit_result_json;
    char edit_error[256];
    IdeIpcEditApplyHandler edit_handler;
    void* edit_handler_userdata;
} IdeIpcServerState;

static IdeIpcServerState g_server = {
    .listen_fd = -1,
    .running = false,
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .socket_path = {0},
    .project_root = {0},
    .session_id = {0},
    .auth_token = {0},
    .started_at_ms = 0,
    .auth_log_last_ms = 0,
    .auth_log_suppressed = 0,
    .peer_reject_log_last_ms = 0,
    .peer_reject_log_suppressed = 0,
    .open_lock = PTHREAD_MUTEX_INITIALIZER,
    .open_cond = PTHREAD_COND_INITIALIZER,
    .open_inflight = false,
    .open_pending = false,
    .open_completed = false,
    .open_success = false,
    .open_line = 0,
    .open_col = 0,
    .open_path = {0},
    .open_error = {0},
    .open_handler = NULL,
    .open_handler_userdata = NULL,
    .edit_lock = PTHREAD_MUTEX_INITIALIZER,
    .edit_cond = PTHREAD_COND_INITIALIZER,
    .edit_inflight = false,
    .edit_pending = false,
    .edit_completed = false,
    .edit_success = false,
    .edit_diff = NULL,
    .edit_result_json = NULL,
    .edit_error = {0},
    .edit_handler = NULL,
    .edit_handler_userdata = NULL,
};

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

static void log_rate_limited_security_event(long long* last_ms,
                                            unsigned* suppressed,
                                            const char* label,
                                            const char* details) {
    if (!last_ms || !suppressed || !label) return;
    long long now = now_ms();
    unsigned suppressed_count = 0;
    bool should_log = false;

    if (*last_ms == 0 || (now - *last_ms) >= IDE_IPC_SECURITY_LOG_INTERVAL_MS) {
        suppressed_count = *suppressed;
        *suppressed = 0;
        *last_ms = now;
        should_log = true;
    } else {
        (*suppressed)++;
    }

    if (!should_log) return;

    if (suppressed_count > 0) {
        fprintf(stderr,
                "[IPC] %s: %s (suppressed %u similar events)\n",
                label,
                (details && details[0]) ? details : "no details",
                suppressed_count);
    } else {
        fprintf(stderr,
                "[IPC] %s: %s\n",
                label,
                (details && details[0]) ? details : "no details");
    }
}

static void note_auth_failure(const char* cmd, const char* reason) {
    char details[160];
    snprintf(details, sizeof(details), "cmd=%s reason=%s",
             (cmd && cmd[0]) ? cmd : "(unknown)",
             (reason && reason[0]) ? reason : "invalid");
    pthread_mutex_lock(&g_server.lock);
    log_rate_limited_security_event(&g_server.auth_log_last_ms,
                                    &g_server.auth_log_suppressed,
                                    "Auth rejected",
                                    details);
    pthread_mutex_unlock(&g_server.lock);
}

static void note_peer_rejection(const char* reason, uid_t peer_uid, bool have_uid) {
    char details[160];
    if (have_uid) {
        snprintf(details, sizeof(details), "reason=%s peer_uid=%u",
                 (reason && reason[0]) ? reason : "rejected",
                 (unsigned)peer_uid);
    } else {
        snprintf(details, sizeof(details), "reason=%s",
                 (reason && reason[0]) ? reason : "rejected");
    }
    pthread_mutex_lock(&g_server.lock);
    log_rate_limited_security_event(&g_server.peer_reject_log_last_ms,
                                    &g_server.peer_reject_log_suppressed,
                                    "Peer rejected",
                                    details);
    pthread_mutex_unlock(&g_server.lock);
}

static bool timing_safe_equal(const char* a, const char* b) {
    if (!a || !b) return false;
    size_t la = strlen(a);
    size_t lb = strlen(b);
    if (la != lb) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < la; ++i) {
        diff |= (unsigned char)(a[i] ^ b[i]);
    }
    return diff == 0;
}

static bool command_requires_auth(const char* cmd) {
    if (!cmd || !*cmd) return false;
    return (strcmp(cmd, "open") == 0 ||
            strcmp(cmd, "build") == 0 ||
            strcmp(cmd, "edit") == 0);
}

static bool request_auth_token_valid(const char* token) {
    if (!token || !*token) return false;
    pthread_mutex_lock(&g_server.lock);
    bool valid = (g_server.auth_token[0] != '\0' &&
                  timing_safe_equal(g_server.auth_token, token));
    pthread_mutex_unlock(&g_server.lock);
    return valid;
}

static bool validate_peer_credentials(int fd) {
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    uid_t peer_uid = 0;
    gid_t peer_gid = 0;
    if (getpeereid(fd, &peer_uid, &peer_gid) != 0) {
        note_peer_rejection("credential_lookup_failed", 0, false);
        return false;
    }
    if (peer_uid != geteuid()) {
        note_peer_rejection("uid_mismatch", peer_uid, true);
        return false;
    }
    return true;
#elif defined(__linux__)
    struct {
        pid_t pid;
        uid_t uid;
        gid_t gid;
    } cred;
    socklen_t len = (socklen_t)sizeof(cred);
    memset(&cred, 0, sizeof(cred));
    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cred, &len) != 0 || len < sizeof(cred)) {
        note_peer_rejection("credential_lookup_failed", 0, false);
        return false;
    }
    if (cred.uid != geteuid()) {
        note_peer_rejection("uid_mismatch", cred.uid, true);
        return false;
    }
    return true;
#else
    (void)fd;
    return true;
#endif
}

static char* handle_request_payload(const char* payload) {
    char req_id[128] = "unknown";

    if (!payload || !*payload) {
        json_object* err = ide_ipc_build_error_obj("bad_request", "Request body is empty", NULL);
        return ide_ipc_build_response_json(req_id, false, NULL, err);
    }

    json_object* root = json_tokener_parse(payload);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        json_object* err = ide_ipc_build_error_obj("bad_json", "Request must be a JSON object", NULL);
        return ide_ipc_build_response_json(req_id, false, NULL, err);
    }

    json_object* j_id = NULL;
    json_object* j_proto = NULL;
    json_object* j_cmd = NULL;
    json_object* j_args = NULL;
    json_object* j_auth = NULL;

    if (json_object_object_get_ex(root, "id", &j_id) && j_id && json_object_is_type(j_id, json_type_string)) {
        str_copy(req_id, sizeof(req_id), json_object_get_string(j_id));
    }

    if (!json_object_object_get_ex(root, "proto", &j_proto) || !j_proto || !json_object_is_type(j_proto, json_type_int)) {
        json_object_put(root);
        json_object* err = ide_ipc_build_error_obj("bad_request", "Missing integer field 'proto'", NULL);
        return ide_ipc_build_response_json(req_id, false, NULL, err);
    }

    int proto = json_object_get_int(j_proto);
    if (proto != IDE_IPC_PROTO_VERSION) {
        json_object_put(root);
        json_object* err = ide_ipc_build_error_obj("bad_proto", "Unsupported protocol version", NULL);
        return ide_ipc_build_response_json(req_id, false, NULL, err);
    }

    if (!json_object_object_get_ex(root, "cmd", &j_cmd) || !j_cmd || !json_object_is_type(j_cmd, json_type_string)) {
        json_object_put(root);
        json_object* err = ide_ipc_build_error_obj("bad_request", "Missing string field 'cmd'", NULL);
        return ide_ipc_build_response_json(req_id, false, NULL, err);
    }

    if (json_object_object_get_ex(root, "args", &j_args) && j_args && !json_object_is_type(j_args, json_type_object)) {
        json_object_put(root);
        json_object* err = ide_ipc_build_error_obj("bad_request", "Field 'args' must be an object", NULL);
        return ide_ipc_build_response_json(req_id, false, NULL, err);
    }

    const char* cmd = json_object_get_string(j_cmd);
    const char* auth_token = NULL;
    if (json_object_object_get_ex(root, "auth_token", &j_auth) &&
        j_auth && json_object_is_type(j_auth, json_type_string)) {
        auth_token = json_object_get_string(j_auth);
    } else if (j_args &&
               json_object_object_get_ex(j_args, "auth_token", &j_auth) &&
               j_auth && json_object_is_type(j_auth, json_type_string)) {
        auth_token = json_object_get_string(j_auth);
    }

    if (command_requires_auth(cmd) && !request_auth_token_valid(auth_token)) {
        note_auth_failure(cmd, (auth_token && auth_token[0]) ? "invalid_token" : "missing_token");
        json_object_put(root);
        json_object* err = ide_ipc_build_error_obj("auth_required",
                                           "Mutating command requires a valid session auth token",
                                           NULL);
        return ide_ipc_build_response_json(req_id, false, NULL, err);
    }

    char* out = NULL;

    if (strcmp(cmd, "ping") == 0) {
        pthread_mutex_lock(&g_server.lock);
        long long uptime = now_ms() - g_server.started_at_ms;
        json_object* result = json_object_new_object();
        json_object_object_add(result, "proto", json_object_new_int(IDE_IPC_PROTO_VERSION));
        json_object_object_add(result, "ide_pid", json_object_new_int((int)getpid()));
        json_object_object_add(result, "session_id", json_object_new_string(g_server.session_id));
        json_object_object_add(result, "project_root", json_object_new_string(g_server.project_root));
        json_object_object_add(result, "socket_path", json_object_new_string(g_server.socket_path));
        json_object_object_add(result, "server_version", json_object_new_string(IDE_IPC_SERVER_VERSION));
        json_object_object_add(result, "started_at_ms", json_object_new_int64(g_server.started_at_ms));
        json_object_object_add(result, "uptime_ms", json_object_new_int64(uptime));
        pthread_mutex_unlock(&g_server.lock);
        out = ide_ipc_build_response_json(req_id, true, result, NULL);
    } else if (strcmp(cmd, "diag") == 0) {
        pthread_mutex_lock(&g_server.lock);
        char project_root[sizeof(g_server.project_root)];
        str_copy(project_root, sizeof(project_root), g_server.project_root);
        pthread_mutex_unlock(&g_server.lock);
        json_object* result = ide_ipc_build_diag_result(j_args, project_root);
        out = ide_ipc_build_response_json(req_id, true, result, NULL);
    } else if (strcmp(cmd, "symbols") == 0) {
        pthread_mutex_lock(&g_server.lock);
        char project_root[sizeof(g_server.project_root)];
        str_copy(project_root, sizeof(project_root), g_server.project_root);
        pthread_mutex_unlock(&g_server.lock);
        json_object* result = ide_ipc_build_symbol_result(j_args, project_root);
        out = ide_ipc_build_response_json(req_id, true, result, NULL);
    } else if (strcmp(cmd, "tokens") == 0) {
        pthread_mutex_lock(&g_server.lock);
        char project_root[sizeof(g_server.project_root)];
        str_copy(project_root, sizeof(project_root), g_server.project_root);
        pthread_mutex_unlock(&g_server.lock);
        json_object* result = ide_ipc_build_token_result(j_args, project_root);
        out = ide_ipc_build_response_json(req_id, true, result, NULL);
    } else if (strcmp(cmd, "includes") == 0) {
        json_object* result = ide_ipc_build_includes_result(j_args);
        out = ide_ipc_build_response_json(req_id, true, result, NULL);
    } else if (strcmp(cmd, "search") == 0) {
        pthread_mutex_lock(&g_server.lock);
        char project_root[sizeof(g_server.project_root)];
        str_copy(project_root, sizeof(project_root), g_server.project_root);
        pthread_mutex_unlock(&g_server.lock);

        json_object* err = NULL;
        json_object* result = ide_ipc_build_search_result(j_args, project_root, &err);
        if (!result) {
            out = ide_ipc_build_response_json(req_id, false, NULL, err);
        } else {
            out = ide_ipc_build_response_json(req_id, true, result, NULL);
        }
    } else if (strcmp(cmd, "build") == 0) {
        pthread_mutex_lock(&g_server.lock);
        char project_root[sizeof(g_server.project_root)];
        str_copy(project_root, sizeof(project_root), g_server.project_root);
        pthread_mutex_unlock(&g_server.lock);

        json_object* err = NULL;
        json_object* result = ide_ipc_build_build_result(j_args, project_root, &err);
        if (!result) {
            out = ide_ipc_build_response_json(req_id, false, NULL, err);
        } else {
            out = ide_ipc_build_response_json(req_id, true, result, NULL);
        }
    } else if (strcmp(cmd, "edit") == 0) {
        json_object *jop=NULL,*jdiff=NULL,*jcheck=NULL,*jhashes=NULL;
        const char* op = NULL;
        const char* diff_text = NULL;
        bool check_hash = true;
        if (j_args && json_object_object_get_ex(j_args, "op", &jop) &&
            jop && json_object_is_type(jop, json_type_string)) {
            op = json_object_get_string(jop);
        }
        if (!op || strcmp(op, "apply") != 0) {
            json_object* err = ide_ipc_build_error_obj("bad_request", "edit requires args.op=\"apply\"", NULL);
            out = ide_ipc_build_response_json(req_id, false, NULL, err);
            json_object_put(root);
            return out;
        }
        if (j_args && json_object_object_get_ex(j_args, "diff", &jdiff) &&
            jdiff && json_object_is_type(jdiff, json_type_string)) {
            diff_text = json_object_get_string(jdiff);
        }
        if (!diff_text || !*diff_text) {
            json_object* err = ide_ipc_build_error_obj("bad_request", "edit apply requires args.diff", NULL);
            out = ide_ipc_build_response_json(req_id, false, NULL, err);
            json_object_put(root);
            return out;
        }
        if (j_args && json_object_object_get_ex(j_args, "check_hash", &jcheck) &&
            jcheck && json_object_is_type(jcheck, json_type_boolean)) {
            check_hash = json_object_get_boolean(jcheck);
        }
        if (j_args) {
            json_object_object_get_ex(j_args, "hashes", &jhashes);
        }

        pthread_mutex_lock(&g_server.lock);
        char project_root[sizeof(g_server.project_root)];
        str_copy(project_root, sizeof(project_root), g_server.project_root);
        pthread_mutex_unlock(&g_server.lock);

        char hash_err[256] = {0};
        if (!ide_ipc_verify_edit_hashes(project_root, diff_text, check_hash, jhashes, hash_err, sizeof(hash_err))) {
            json_object* err = ide_ipc_build_error_obj("hash_mismatch", hash_err[0] ? hash_err : "Hash verification failed", NULL);
            out = ide_ipc_build_response_json(req_id, false, NULL, err);
            json_object_put(root);
            return out;
        }

        pthread_mutex_lock(&g_server.edit_lock);
        if (!g_server.edit_handler) {
            pthread_mutex_unlock(&g_server.edit_lock);
            json_object* err = ide_ipc_build_error_obj("edit_unavailable", "IDE edit handler is not configured", NULL);
            out = ide_ipc_build_response_json(req_id, false, NULL, err);
            json_object_put(root);
            return out;
        }
        if (g_server.edit_inflight) {
            pthread_mutex_unlock(&g_server.edit_lock);
            json_object* err = ide_ipc_build_error_obj("edit_busy", "Another edit apply request is in flight", NULL);
            out = ide_ipc_build_response_json(req_id, false, NULL, err);
            json_object_put(root);
            return out;
        }

        g_server.edit_inflight = true;
        g_server.edit_pending = true;
        g_server.edit_completed = false;
        g_server.edit_success = false;
        free(g_server.edit_diff);
        g_server.edit_diff = strdup(diff_text);
        free(g_server.edit_result_json);
        g_server.edit_result_json = NULL;
        g_server.edit_error[0] = '\0';

        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 8;

        while (!g_server.edit_completed && g_server.running) {
            int wait_res = pthread_cond_timedwait(&g_server.edit_cond, &g_server.edit_lock, &ts);
            if (wait_res == ETIMEDOUT) {
                str_copy(g_server.edit_error, sizeof(g_server.edit_error), "Timed out waiting for IDE edit apply");
                g_server.edit_pending = false;
                g_server.edit_completed = true;
                g_server.edit_success = false;
                break;
            }
        }

        bool ok_edit = g_server.edit_success;
        char edit_error[sizeof(g_server.edit_error)];
        str_copy(edit_error, sizeof(edit_error), g_server.edit_error);
        char* result_json = g_server.edit_result_json ? strdup(g_server.edit_result_json) : NULL;
        g_server.edit_inflight = false;
        pthread_mutex_unlock(&g_server.edit_lock);

        if (ok_edit) {
            json_object* result = NULL;
            if (result_json) {
                result = json_tokener_parse(result_json);
            }
            if (!result || !json_object_is_type(result, json_type_object)) {
                if (result) json_object_put(result);
                result = json_object_new_object();
            }
            json_object_object_add(result, "applied", json_object_new_boolean(true));
            json_object_object_add(result, "hash_check", json_object_new_boolean(check_hash));
            out = ide_ipc_build_response_json(req_id, true, result, NULL);
        } else {
            json_object* err = ide_ipc_build_error_obj("edit_failed",
                                               edit_error[0] ? edit_error : "Failed to apply edit patch",
                                               NULL);
            out = ide_ipc_build_response_json(req_id, false, NULL, err);
        }
        free(result_json);
    } else if (strcmp(cmd, "open") == 0) {
        json_object* jpath = NULL;
        json_object* jline = NULL;
        json_object* jcol = NULL;
        if (!j_args ||
            !json_object_object_get_ex(j_args, "path", &jpath) ||
            !jpath || !json_object_is_type(jpath, json_type_string) ||
            !json_object_object_get_ex(j_args, "line", &jline) ||
            !jline || !json_object_is_type(jline, json_type_int) ||
            !json_object_object_get_ex(j_args, "col", &jcol) ||
            !jcol || !json_object_is_type(jcol, json_type_int)) {
            json_object_put(root);
            json_object* err = ide_ipc_build_error_obj("bad_request",
                                               "open requires args.path (string), args.line (int), args.col (int)",
                                               NULL);
            return ide_ipc_build_response_json(req_id, false, NULL, err);
        }

        const char* req_path = json_object_get_string(jpath);
        int req_line = json_object_get_int(jline);
        int req_col = json_object_get_int(jcol);
        if (!req_path || !req_path[0]) {
            json_object_put(root);
            json_object* err = ide_ipc_build_error_obj("bad_request", "open path is empty", NULL);
            return ide_ipc_build_response_json(req_id, false, NULL, err);
        }

        pthread_mutex_lock(&g_server.open_lock);
        if (!g_server.open_handler) {
            pthread_mutex_unlock(&g_server.open_lock);
            json_object* err = ide_ipc_build_error_obj("open_unavailable",
                                               "IDE open handler is not configured",
                                               NULL);
            out = ide_ipc_build_response_json(req_id, false, NULL, err);
        } else if (g_server.open_inflight) {
            pthread_mutex_unlock(&g_server.open_lock);
            json_object* err = ide_ipc_build_error_obj("open_busy",
                                               "Another open request is in flight",
                                               NULL);
            out = ide_ipc_build_response_json(req_id, false, NULL, err);
        } else {
            g_server.open_inflight = true;
            g_server.open_pending = true;
            g_server.open_completed = false;
            g_server.open_success = false;
            g_server.open_line = req_line;
            g_server.open_col = req_col;
            str_copy(g_server.open_path, sizeof(g_server.open_path), req_path);
            g_server.open_error[0] = '\0';

            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 5;

            while (!g_server.open_completed && g_server.running) {
                int wait_res = pthread_cond_timedwait(&g_server.open_cond, &g_server.open_lock, &ts);
                if (wait_res == ETIMEDOUT) {
                    str_copy(g_server.open_error, sizeof(g_server.open_error), "Timed out waiting for IDE open apply");
                    g_server.open_pending = false;
                    g_server.open_completed = true;
                    g_server.open_success = false;
                    break;
                }
            }

            bool ok_open = g_server.open_success;
            char open_error[sizeof(g_server.open_error)];
            str_copy(open_error, sizeof(open_error), g_server.open_error);
            g_server.open_inflight = false;
            pthread_mutex_unlock(&g_server.open_lock);

            if (ok_open) {
                json_object* result = json_object_new_object();
                json_object_object_add(result, "path", json_object_new_string(req_path));
                json_object_object_add(result, "line", json_object_new_int(req_line));
                json_object_object_add(result, "col", json_object_new_int(req_col));
                json_object_object_add(result, "applied", json_object_new_boolean(true));
                out = ide_ipc_build_response_json(req_id, true, result, NULL);
            } else {
                json_object* err = ide_ipc_build_error_obj("open_failed",
                                                   open_error[0] ? open_error : "Failed to apply open request",
                                                   NULL);
                out = ide_ipc_build_response_json(req_id, false, NULL, err);
            }
        }
    } else {
        json_object* err = ide_ipc_build_error_obj("unknown_cmd", "Unknown command", cmd);
        out = ide_ipc_build_response_json(req_id, false, NULL, err);
    }

    json_object_put(root);
    return out;
}

typedef struct {
    int fd;
} ClientCtx;

static void* client_thread_main(void* arg) {
    ClientCtx* ctx = (ClientCtx*)arg;
    if (!ctx) return NULL;

    int fd = ctx->fd;
    free(ctx);

    char* buf = (char*)malloc(IDE_IPC_MAX_REQUEST_BYTES + 1);
    if (!buf) {
        close(fd);
        return NULL;
    }

    size_t used = 0;
    while (used < IDE_IPC_MAX_REQUEST_BYTES) {
        ssize_t n = read(fd, buf + used, IDE_IPC_MAX_REQUEST_BYTES - used);
        if (n > 0) {
            used += (size_t)n;
            char* nl = memchr(buf, '\n', used);
            if (nl) {
                used = (size_t)(nl - buf);
                break;
            }
            continue;
        }
        if (n == 0) break;
        if (errno == EINTR) continue;
        break;
    }
    buf[used] = '\0';

    char* response = handle_request_payload(buf);
    if (response) {
        size_t left = strlen(response);
        const char* p = response;
        while (left > 0) {
            ssize_t wn = write(fd, p, left);
            if (wn > 0) {
                p += wn;
                left -= (size_t)wn;
                continue;
            }
            if (wn < 0 && errno == EINTR) continue;
            break;
        }
        free(response);
    }

    free(buf);
    close(fd);
    return NULL;
}

static void* accept_thread_main(void* arg) {
    (void)arg;
    for (;;) {
        pthread_mutex_lock(&g_server.lock);
        bool running = g_server.running;
        int listen_fd = g_server.listen_fd;
        pthread_mutex_unlock(&g_server.lock);

        if (!running || listen_fd < 0) break;

        int client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            pthread_mutex_lock(&g_server.lock);
            bool still_running = g_server.running;
            pthread_mutex_unlock(&g_server.lock);
            if (!still_running) break;
            continue;
        }

        if (!validate_peer_credentials(client_fd)) {
            close(client_fd);
            continue;
        }

        ClientCtx* ctx = (ClientCtx*)calloc(1, sizeof(ClientCtx));
        if (!ctx) {
            close(client_fd);
            continue;
        }
        ctx->fd = client_fd;

        pthread_t client_thread;
        if (pthread_create(&client_thread, NULL, client_thread_main, ctx) == 0) {
            pthread_detach(client_thread);
        } else {
            close(client_fd);
            free(ctx);
        }
    }

    return NULL;
}

static void close_server_fd_locked(void) {
    if (g_server.listen_fd >= 0) {
        shutdown(g_server.listen_fd, SHUT_RDWR);
        close(g_server.listen_fd);
        g_server.listen_fd = -1;
    }
}

bool ide_ipc_start(const char* project_root) {
    pthread_mutex_lock(&g_server.lock);
    if (g_server.running) {
        pthread_mutex_unlock(&g_server.lock);
        return true;
    }
    pthread_mutex_unlock(&g_server.lock);

    char socket_path[108] = {0};
    char session_id[128] = {0};
    char auth_token[65] = {0};
    if (!ide_ipc_build_socket_path(socket_path, sizeof(socket_path), session_id, sizeof(session_id))) {
        fprintf(stderr, "[IPC] Failed to prepare socket path.\n");
        return false;
    }
    if (!ide_ipc_generate_auth_token_hex(auth_token)) {
        fprintf(stderr, "[IPC] Failed to generate strong auth token.\n");
        return false;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("[IPC] socket");
        return false;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path);

    unlink(socket_path);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        perror("[IPC] bind");
        close(fd);
        return false;
    }

    if (chmod(socket_path, 0600) != 0) {
        perror("[IPC] chmod");
        close(fd);
        unlink(socket_path);
        return false;
    }

    if (listen(fd, 16) != 0) {
        perror("[IPC] listen");
        close(fd);
        unlink(socket_path);
        return false;
    }

    pthread_mutex_lock(&g_server.lock);
    g_server.listen_fd = fd;
    g_server.running = true;
    str_copy(g_server.socket_path, sizeof(g_server.socket_path), socket_path);
    str_copy(g_server.session_id, sizeof(g_server.session_id), session_id);
    str_copy(g_server.project_root, sizeof(g_server.project_root), project_root ? project_root : "");
    str_copy(g_server.auth_token, sizeof(g_server.auth_token), auth_token);
    g_server.started_at_ms = now_ms();
    g_server.auth_log_last_ms = 0;
    g_server.auth_log_suppressed = 0;
    g_server.peer_reject_log_last_ms = 0;
    g_server.peer_reject_log_suppressed = 0;
    pthread_mutex_unlock(&g_server.lock);

    if (pthread_create(&g_server.accept_thread, NULL, accept_thread_main, NULL) != 0) {
        perror("[IPC] pthread_create");
        pthread_mutex_lock(&g_server.lock);
        g_server.running = false;
        close_server_fd_locked();
        char stale_path[108];
        str_copy(stale_path, sizeof(stale_path), g_server.socket_path);
        g_server.socket_path[0] = '\0';
        g_server.session_id[0] = '\0';
        g_server.project_root[0] = '\0';
        g_server.auth_token[0] = '\0';
        g_server.started_at_ms = 0;
        g_server.auth_log_last_ms = 0;
        g_server.auth_log_suppressed = 0;
        g_server.peer_reject_log_last_ms = 0;
        g_server.peer_reject_log_suppressed = 0;
        pthread_mutex_unlock(&g_server.lock);
        if (stale_path[0]) unlink(stale_path);
        return false;
    }

    printf("[IPC] Listening on %s\n", g_server.socket_path);
    return true;
}

void ide_ipc_stop(void) {
    pthread_mutex_lock(&g_server.lock);
    bool was_running = g_server.running;
    g_server.running = false;
    close_server_fd_locked();
    char socket_path[108];
    str_copy(socket_path, sizeof(socket_path), g_server.socket_path);
    g_server.socket_path[0] = '\0';
    g_server.session_id[0] = '\0';
    g_server.project_root[0] = '\0';
    g_server.auth_token[0] = '\0';
    g_server.started_at_ms = 0;
    g_server.auth_log_last_ms = 0;
    g_server.auth_log_suppressed = 0;
    g_server.peer_reject_log_last_ms = 0;
    g_server.peer_reject_log_suppressed = 0;
    pthread_mutex_unlock(&g_server.lock);

    pthread_mutex_lock(&g_server.open_lock);
    g_server.open_pending = false;
    g_server.open_completed = true;
    g_server.open_success = false;
    g_server.open_inflight = false;
    g_server.open_error[0] = '\0';
    pthread_cond_broadcast(&g_server.open_cond);
    pthread_mutex_unlock(&g_server.open_lock);

    pthread_mutex_lock(&g_server.edit_lock);
    g_server.edit_pending = false;
    g_server.edit_completed = true;
    g_server.edit_success = false;
    g_server.edit_inflight = false;
    g_server.edit_error[0] = '\0';
    free(g_server.edit_diff);
    g_server.edit_diff = NULL;
    free(g_server.edit_result_json);
    g_server.edit_result_json = NULL;
    pthread_cond_broadcast(&g_server.edit_cond);
    pthread_mutex_unlock(&g_server.edit_lock);

    if (was_running) {
        pthread_join(g_server.accept_thread, NULL);
    }

    if (socket_path[0]) {
        unlink(socket_path);
    }
}

bool ide_ipc_is_running(void) {
    pthread_mutex_lock(&g_server.lock);
    bool running = g_server.running;
    pthread_mutex_unlock(&g_server.lock);
    return running;
}

void ide_ipc_set_open_handler(IdeIpcOpenHandler handler, void* userdata) {
    pthread_mutex_lock(&g_server.open_lock);
    g_server.open_handler = handler;
    g_server.open_handler_userdata = userdata;
    pthread_mutex_unlock(&g_server.open_lock);
}

void ide_ipc_set_edit_handler(IdeIpcEditApplyHandler handler, void* userdata) {
    pthread_mutex_lock(&g_server.edit_lock);
    g_server.edit_handler = handler;
    g_server.edit_handler_userdata = userdata;
    pthread_mutex_unlock(&g_server.edit_lock);
}

void ide_ipc_pump(void) {
    IdeIpcOpenHandler handler = NULL;
    void* handler_userdata = NULL;
    char path[1024] = {0};
    int line = 0;
    int col = 0;
    bool have_open_request = false;

    pthread_mutex_lock(&g_server.open_lock);
    if (g_server.open_pending && g_server.open_inflight) {
        handler = g_server.open_handler;
        handler_userdata = g_server.open_handler_userdata;
        str_copy(path, sizeof(path), g_server.open_path);
        line = g_server.open_line;
        col = g_server.open_col;
        g_server.open_pending = false;
        have_open_request = true;
    }
    pthread_mutex_unlock(&g_server.open_lock);

    if (have_open_request) {
        bool ok = false;
        char error[256] = {0};
        if (handler) {
            ok = handler(path, line, col, error, sizeof(error), handler_userdata);
        } else {
            str_copy(error, sizeof(error), "No IDE open handler is configured");
        }

        pthread_mutex_lock(&g_server.open_lock);
        g_server.open_success = ok;
        g_server.open_completed = true;
        str_copy(g_server.open_error, sizeof(g_server.open_error), error);
        pthread_cond_broadcast(&g_server.open_cond);
        pthread_mutex_unlock(&g_server.open_lock);
    }

    IdeIpcEditApplyHandler edit_handler = NULL;
    void* edit_userdata = NULL;
    char* diff_text = NULL;
    pthread_mutex_lock(&g_server.edit_lock);
    if (g_server.edit_pending && g_server.edit_inflight) {
        edit_handler = g_server.edit_handler;
        edit_userdata = g_server.edit_handler_userdata;
        diff_text = g_server.edit_diff ? strdup(g_server.edit_diff) : NULL;
        g_server.edit_pending = false;
    }
    pthread_mutex_unlock(&g_server.edit_lock);

    if (edit_handler && diff_text) {
        bool ok_edit = false;
        char edit_err[256] = {0};
        json_object* result = NULL;
        ok_edit = edit_handler(diff_text, edit_err, sizeof(edit_err), edit_userdata, &result);
        free(diff_text);

        char* result_json = NULL;
        if (result) {
            const char* raw = json_object_to_json_string_ext(result, JSON_C_TO_STRING_PLAIN);
            if (raw) result_json = strdup(raw);
            json_object_put(result);
        }

        pthread_mutex_lock(&g_server.edit_lock);
        g_server.edit_success = ok_edit;
        g_server.edit_completed = true;
        str_copy(g_server.edit_error, sizeof(g_server.edit_error), edit_err);
        free(g_server.edit_result_json);
        g_server.edit_result_json = result_json;
        pthread_cond_broadcast(&g_server.edit_cond);
        pthread_mutex_unlock(&g_server.edit_lock);
    } else if (diff_text) {
        free(diff_text);
    }
}

const char* ide_ipc_socket_path(void) {
    static char out[108];
    pthread_mutex_lock(&g_server.lock);
    str_copy(out, sizeof(out), g_server.socket_path);
    pthread_mutex_unlock(&g_server.lock);
    return out;
}

const char* ide_ipc_session_id(void) {
    static char out[128];
    pthread_mutex_lock(&g_server.lock);
    str_copy(out, sizeof(out), g_server.session_id);
    pthread_mutex_unlock(&g_server.lock);
    return out;
}

const char* ide_ipc_auth_token(void) {
    static char out[65];
    pthread_mutex_lock(&g_server.lock);
    str_copy(out, sizeof(out), g_server.auth_token);
    pthread_mutex_unlock(&g_server.lock);
    return out;
}
