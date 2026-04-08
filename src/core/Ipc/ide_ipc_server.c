#include "ide_ipc_server.h"
#include "core/BuildSystem/build_diagnostics.h"
#include "core/Diagnostics/diagnostics_engine.h"
#include "core/Analysis/analysis_symbols_store.h"
#include "core/Analysis/analysis_token_store.h"
#include "core/Analysis/library_index.h"
#include "app/GlobalInfo/workspace_prefs.h"
#include "core/Ipc/ide_ipc_edit_apply.h"
#include "core/Ipc/ide_ipc_path_guard.h"

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

static bool generate_auth_token_hex(char out[65]) {
    if (!out) return false;
    unsigned char buf[32];
    memset(buf, 0, sizeof(buf));

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

static json_object* build_error_obj(const char* code, const char* message, const char* details);

static bool ensure_dir_exists(const char* path, mode_t mode) {
    if (!path || !*path) return false;
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    if (mkdir(path, mode) == 0) return true;
    return errno == EEXIST;
}

static bool build_socket_path(char* out, size_t out_cap, char* out_session, size_t session_cap) {
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

    // UNIX path length is limited by sockaddr_un.sun_path.
    if (strlen(candidate) >= out_cap) {
        snprintf(candidate, sizeof(candidate), "/tmp/%s_%d.sock", IDE_IPC_APP_DIR, (int)pid);
        if (strlen(candidate) >= out_cap) return false;
    }

    snprintf(out, out_cap, "%s", candidate);
    return true;
}

static const char* symbol_kind_name(FisicsSymbolKind kind) {
    switch (kind) {
        case FISICS_SYMBOL_FUNCTION: return "function";
        case FISICS_SYMBOL_STRUCT: return "struct";
        case FISICS_SYMBOL_UNION: return "union";
        case FISICS_SYMBOL_ENUM: return "enum";
        case FISICS_SYMBOL_TYPEDEF: return "typedef";
        case FISICS_SYMBOL_VARIABLE: return "variable";
        case FISICS_SYMBOL_FIELD: return "field";
        case FISICS_SYMBOL_ENUM_MEMBER: return "enum_member";
        case FISICS_SYMBOL_MACRO: return "macro";
        default: return "unknown";
    }
}

static const char* token_kind_name(FisicsTokenKind kind) {
    switch (kind) {
        case FISICS_TOK_IDENTIFIER: return "identifier";
        case FISICS_TOK_KEYWORD: return "keyword";
        case FISICS_TOK_NUMBER: return "number";
        case FISICS_TOK_STRING: return "string";
        case FISICS_TOK_CHAR: return "char";
        case FISICS_TOK_OPERATOR: return "operator";
        case FISICS_TOK_PUNCT: return "punct";
        case FISICS_TOK_COMMENT: return "comment";
        case FISICS_TOK_WHITESPACE: return "whitespace";
        default: return "unknown";
    }
}

static void symbol_stable_id_hex(uint64_t value, char out[19]) {
    if (!out) return;
    snprintf(out, 19, "0x%016llx", (unsigned long long)value);
}

static const char* normalize_or_project_path(const char* input,
                                             const char* project_root,
                                             char* out,
                                             size_t out_cap) {
    if (!input || !*input || !out || out_cap == 0) return "";
    if (input[0] == '/') {
        snprintf(out, out_cap, "%s", input);
    } else if (project_root && *project_root) {
        snprintf(out, out_cap, "%s/%s", project_root, input);
    } else {
        snprintf(out, out_cap, "%s", input);
    }
    return out;
}

static bool symbol_is_top_level(const FisicsSymbol* sym) {
    if (!sym) return false;
    return (!sym->parent_name || !sym->parent_name[0]);
}

typedef enum {
    DIAG_TAXONOMY_INFO = 0,
    DIAG_TAXONOMY_WARNING = 1,
    DIAG_TAXONOMY_ERROR = 2
} DiagTaxonomySeverity;

static const char* diag_severity_legacy_name(DiagTaxonomySeverity sev) {
    switch (sev) {
        case DIAG_TAXONOMY_ERROR: return "error";
        case DIAG_TAXONOMY_WARNING: return "warn";
        default: return "info";
    }
}

static const char* diag_severity_normalized_name(DiagTaxonomySeverity sev) {
    switch (sev) {
        case DIAG_TAXONOMY_ERROR: return "error";
        case DIAG_TAXONOMY_WARNING: return "warning";
        default: return "info";
    }
}

static void add_diag_taxonomy_fields(json_object* d,
                                     DiagTaxonomySeverity sev,
                                     const char* category,
                                     const char* code_text,
                                     int code_id) {
    if (!d) return;
    const char* safe_category = (category && category[0]) ? category : "unknown";
    const char* safe_code = (code_text && code_text[0]) ? code_text : "";
    json_object_object_add(d, "severity", json_object_new_string(diag_severity_legacy_name(sev)));
    json_object_object_add(d, "severity_name", json_object_new_string(diag_severity_normalized_name(sev)));
    json_object_object_add(d, "severity_id", json_object_new_int((int)sev));
    json_object_object_add(d, "category", json_object_new_string(safe_category));
    json_object_object_add(d, "code", json_object_new_string(safe_code));
    json_object_object_add(d, "code_id", json_object_new_int(code_id));
}

static json_object* build_diag_result(json_object* args, const char* project_root) {
    (void)project_root;
    int max_items = -1;
    if (args) {
        json_object* jmax = NULL;
        if (json_object_object_get_ex(args, "max", &jmax) &&
            jmax &&
            json_object_is_type(jmax, json_type_int)) {
            max_items = json_object_get_int(jmax);
            if (max_items < 0) max_items = -1;
        }
    }

    json_object* result = json_object_new_object();
    json_object* summary = json_object_new_object();
    json_object* arr = json_object_new_array();

    int total = 0;
    int err = 0;
    int warn = 0;
    int info = 0;
    int returned = 0;

    size_t build_count = 0;
    const BuildDiagnostic* build = build_diagnostics_get(&build_count);
    for (size_t i = 0; i < build_count; ++i) {
        total++;
        if (build[i].isError) err++; else warn++;
        if (max_items >= 0 && returned >= max_items) continue;
        json_object* d = json_object_new_object();
        json_object_object_add(d, "file", json_object_new_string(build[i].path));
        json_object_object_add(d, "line", json_object_new_int(build[i].line));
        json_object_object_add(d, "col", json_object_new_int(build[i].col));
        json_object_object_add(d, "endLine", json_object_new_int(build[i].line));
        json_object_object_add(d, "endCol", json_object_new_int(build[i].col + 1));
        json_object_object_add(d, "message", json_object_new_string(build[i].message));
        add_diag_taxonomy_fields(d,
                                 build[i].isError ? DIAG_TAXONOMY_ERROR : DIAG_TAXONOMY_WARNING,
                                 "build",
                                 "",
                                 0);
        json_object_object_add(d, "source", json_object_new_string("build"));
        json_object_array_add(arr, d);
        returned++;
    }

    int diag_count = getDiagnosticCount();
    for (int i = 0; i < diag_count; ++i) {
        const Diagnostic* dg = getDiagnosticAt(i);
        if (!dg) continue;
        total++;
        if (dg->severity == DIAG_SEVERITY_ERROR) err++;
        else if (dg->severity == DIAG_SEVERITY_WARNING) warn++;
        else info++;

        if (max_items >= 0 && returned >= max_items) continue;
        DiagTaxonomySeverity sev = DIAG_TAXONOMY_INFO;
        if (dg->severity == DIAG_SEVERITY_ERROR) sev = DIAG_TAXONOMY_ERROR;
        else if (dg->severity == DIAG_SEVERITY_WARNING) sev = DIAG_TAXONOMY_WARNING;
        json_object* d = json_object_new_object();
        json_object_object_add(d, "file", json_object_new_string(dg->filePath ? dg->filePath : ""));
        json_object_object_add(d, "line", json_object_new_int(dg->line));
        json_object_object_add(d, "col", json_object_new_int(dg->column));
        json_object_object_add(d, "endLine", json_object_new_int(dg->line));
        json_object_object_add(d, "endCol", json_object_new_int(dg->column + 1));
        json_object_object_add(d, "message", json_object_new_string(dg->message ? dg->message : ""));
        char code_text[32];
        snprintf(code_text, sizeof(code_text), "%d", dg->codeId);
        add_diag_taxonomy_fields(d,
                                 sev,
                                 diagnostic_category_name(dg->category),
                                 dg->codeId ? code_text : "",
                                 dg->codeId);
        json_object_object_add(d, "source", json_object_new_string("analysis"));
        json_object_array_add(arr, d);
        returned++;
    }

    json_object_object_add(summary, "total", json_object_new_int(total));
    json_object_object_add(summary, "error", json_object_new_int(err));
    json_object_object_add(summary, "warn", json_object_new_int(warn));
    json_object_object_add(summary, "info", json_object_new_int(info));

    json_object_object_add(result, "summary", summary);
    json_object_object_add(result, "diagnostics", arr);
    json_object_object_add(result, "total_count", json_object_new_int(total));
    json_object_object_add(result, "returned_count", json_object_new_int(returned));
    json_object_object_add(result, "truncated", json_object_new_boolean(max_items >= 0 && returned < total));
    return result;
}

static json_object* build_symbol_result(json_object* args, const char* project_root) {
    int max_items = -1;
    bool top_only = false;
    char file_filter[1024] = {0};
    if (args) {
        json_object* jmax = NULL;
        json_object* jtop = NULL;
        json_object* jfile = NULL;
        if (json_object_object_get_ex(args, "max", &jmax) &&
            jmax && json_object_is_type(jmax, json_type_int)) {
            max_items = json_object_get_int(jmax);
            if (max_items < 0) max_items = -1;
        }
        if (json_object_object_get_ex(args, "top_level_only", &jtop) &&
            jtop && json_object_is_type(jtop, json_type_boolean)) {
            top_only = json_object_get_boolean(jtop);
        }
        if (json_object_object_get_ex(args, "file", &jfile) &&
            jfile && json_object_is_type(jfile, json_type_string)) {
            normalize_or_project_path(json_object_get_string(jfile), project_root, file_filter, sizeof(file_filter));
        }
    }

    json_object* result = json_object_new_object();
    json_object* arr = json_object_new_array();

    int total = 0;
    int returned = 0;
    analysis_symbols_store_lock();
    size_t file_count = analysis_symbols_store_file_count();
    for (size_t fi = 0; fi < file_count; ++fi) {
        const AnalysisFileSymbols* file_entry = analysis_symbols_store_file_at(fi);
        if (!file_entry) continue;
        if (file_filter[0]) {
            const char* path = file_entry->path ? file_entry->path : "";
            if (strcmp(path, file_filter) != 0) continue;
        }
        for (size_t si = 0; si < file_entry->count; ++si) {
            const FisicsSymbol* sym = &file_entry->symbols[si];
            if (top_only && !symbol_is_top_level(sym)) continue;
            total++;
            if (max_items >= 0 && returned >= max_items) continue;

            json_object* s = json_object_new_object();
            json_object_object_add(s, "kind", json_object_new_string(symbol_kind_name(sym->kind)));
            json_object_object_add(s, "kind_id", json_object_new_int((int)sym->kind));
            json_object_object_add(s, "name", json_object_new_string(sym->name ? sym->name : ""));
            char stable_id_hex[19];
            symbol_stable_id_hex(sym->stable_id, stable_id_hex);
            json_object_object_add(s, "stable_id", json_object_new_string(stable_id_hex));
            char parent_stable_id_hex[19];
            symbol_stable_id_hex(sym->parent_stable_id, parent_stable_id_hex);
            json_object_object_add(s, "parent_stable_id", json_object_new_string(parent_stable_id_hex));
            json_object_object_add(s, "file", json_object_new_string(sym->file_path ? sym->file_path :
                                                                     (file_entry->path ? file_entry->path : "")));
            json_object_object_add(s, "start_line", json_object_new_int(sym->start_line));
            json_object_object_add(s, "start_col", json_object_new_int(sym->start_col));
            json_object_object_add(s, "end_line", json_object_new_int(sym->end_line));
            json_object_object_add(s, "end_col", json_object_new_int(sym->end_col));
            json_object_object_add(s, "return_type", json_object_new_string(sym->return_type ? sym->return_type : ""));
            json_object_object_add(s, "owner", json_object_new_string(sym->parent_name ? sym->parent_name : ""));
            json_object_object_add(s, "parent_kind", json_object_new_string(symbol_kind_name(sym->parent_kind)));
            json_object_object_add(s, "is_definition", json_object_new_boolean(sym->is_definition));
            json_object_object_add(s, "is_variadic", json_object_new_boolean(sym->is_variadic));

            char signature[1024];
            signature[0] = '\0';
            if (sym->return_type && sym->return_type[0]) {
                snprintf(signature, sizeof(signature), "%s %s(", sym->return_type, sym->name ? sym->name : "");
            } else {
                snprintf(signature, sizeof(signature), "%s(", sym->name ? sym->name : "");
            }
            for (size_t pi = 0; pi < sym->param_count; ++pi) {
                const char* ptype = (sym->param_types && sym->param_types[pi]) ? sym->param_types[pi] : "";
                const char* pname = (sym->param_names && sym->param_names[pi]) ? sym->param_names[pi] : "";
                char piece[128];
                if (pi > 0) strncat(signature, ", ", sizeof(signature) - strlen(signature) - 1);
                if (ptype[0] && pname[0]) snprintf(piece, sizeof(piece), "%s %s", ptype, pname);
                else if (ptype[0]) snprintf(piece, sizeof(piece), "%s", ptype);
                else snprintf(piece, sizeof(piece), "%s", pname);
                strncat(signature, piece, sizeof(signature) - strlen(signature) - 1);
            }
            strncat(signature, ")", sizeof(signature) - strlen(signature) - 1);
            json_object_object_add(s, "signature", json_object_new_string(signature));

            json_object_array_add(arr, s);
            returned++;
        }
    }
    analysis_symbols_store_unlock();

    json_object_object_add(result, "symbols", arr);
    json_object_object_add(result, "total_count", json_object_new_int(total));
    json_object_object_add(result, "returned_count", json_object_new_int(returned));
    json_object_object_add(result, "truncated", json_object_new_boolean(max_items >= 0 && returned < total));
    return result;
}

static json_object* build_token_result(json_object* args, const char* project_root) {
    int max_items = -1;
    char file_filter[1024] = {0};
    if (args) {
        json_object* jmax = NULL;
        json_object* jfile = NULL;
        if (json_object_object_get_ex(args, "max", &jmax) &&
            jmax && json_object_is_type(jmax, json_type_int)) {
            max_items = json_object_get_int(jmax);
            if (max_items < 0) max_items = -1;
        }
        if (json_object_object_get_ex(args, "file", &jfile) &&
            jfile && json_object_is_type(jfile, json_type_string)) {
            normalize_or_project_path(json_object_get_string(jfile), project_root, file_filter, sizeof(file_filter));
        }
    }

    json_object* result = json_object_new_object();
    json_object* arr = json_object_new_array();

    int total = 0;
    int returned = 0;
    size_t file_count = analysis_token_store_file_count();
    for (size_t fi = 0; fi < file_count; ++fi) {
        const AnalysisFileTokens* file_entry = analysis_token_store_file_at(fi);
        if (!file_entry) continue;
        const char* file_path = file_entry->path ? file_entry->path : "";
        if (file_filter[0] && strcmp(file_path, file_filter) != 0) continue;
        for (size_t ti = 0; ti < file_entry->count; ++ti) {
            const FisicsTokenSpan* tok = &file_entry->spans[ti];
            total++;
            if (max_items >= 0 && returned >= max_items) continue;

            json_object* t = json_object_new_object();
            json_object_object_add(t, "file", json_object_new_string(file_path));
            json_object_object_add(t, "line", json_object_new_int(tok->line));
            json_object_object_add(t, "column", json_object_new_int(tok->column));
            json_object_object_add(t, "length", json_object_new_int(tok->length));
            json_object_object_add(t, "kind", json_object_new_string(token_kind_name(tok->kind)));
            json_object_object_add(t, "kind_id", json_object_new_int((int)tok->kind));
            json_object_array_add(arr, t);
            returned++;
        }
    }

    json_object_object_add(result, "tokens", arr);
    json_object_object_add(result, "total_count", json_object_new_int(total));
    json_object_object_add(result, "returned_count", json_object_new_int(returned));
    json_object_object_add(result, "truncated", json_object_new_boolean(max_items >= 0 && returned < total));
    return result;
}

static const char* bucket_kind_name(LibraryBucketKind kind) {
    switch (kind) {
        case LIB_BUCKET_PROJECT: return "project";
        case LIB_BUCKET_SYSTEM: return "system";
        case LIB_BUCKET_EXTERNAL: return "external";
        case LIB_BUCKET_UNRESOLVED: return "unresolved";
        default: return "unknown";
    }
}

static json_object* build_includes_result(json_object* args) {
    bool want_graph = false;
    if (args) {
        json_object* jgraph = NULL;
        if (json_object_object_get_ex(args, "graph", &jgraph) &&
            jgraph && json_object_is_type(jgraph, json_type_boolean)) {
            want_graph = json_object_get_boolean(jgraph);
        }
    }

    json_object* result = json_object_new_object();
    json_object* buckets_arr = json_object_new_array();
    json_object* summary = json_object_new_object();
    json_object* edges = want_graph ? json_object_new_array() : NULL;

    int total_headers = 0;
    int total_usages = 0;
    int bucket_headers[LIB_BUCKET_COUNT] = {0};

    library_index_lock();
    size_t bucket_count = library_index_bucket_count();
    for (size_t bi = 0; bi < bucket_count; ++bi) {
        const LibraryBucket* bucket = library_index_get_bucket(bi);
        if (!bucket) continue;

        json_object* jb = json_object_new_object();
        json_object_object_add(jb, "kind", json_object_new_string(bucket_kind_name(bucket->kind)));
        json_object_object_add(jb, "kind_id", json_object_new_int((int)bucket->kind));

        json_object* headers_arr = json_object_new_array();
        size_t header_count = library_index_header_count(bucket);
        for (size_t hi = 0; hi < header_count; ++hi) {
            const LibraryHeader* header = library_index_get_header(bucket, hi);
            if (!header) continue;
            total_headers++;
            if (bucket->kind >= 0 && bucket->kind < LIB_BUCKET_COUNT) {
                bucket_headers[bucket->kind]++;
            }

            json_object* jh = json_object_new_object();
            json_object_object_add(jh, "name", json_object_new_string(header->name ? header->name : ""));
            json_object_object_add(jh, "resolved_path", json_object_new_string(header->resolved_path ? header->resolved_path : ""));
            json_object_object_add(jh, "include_kind",
                                   json_object_new_string(header->kind == LIB_INCLUDE_KIND_SYSTEM ? "system" : "local"));
            json_object_object_add(jh, "origin", json_object_new_string(bucket_kind_name(header->origin)));
            json_object_object_add(jh, "origin_id", json_object_new_int((int)header->origin));

            json_object* usages_arr = json_object_new_array();
            size_t usage_count = library_index_usage_count(header);
            json_object_object_add(jh, "usage_count", json_object_new_int((int)usage_count));
            for (size_t ui = 0; ui < usage_count; ++ui) {
                const LibraryUsage* usage = library_index_get_usage(header, ui);
                if (!usage) continue;
                total_usages++;
                json_object* ju = json_object_new_object();
                json_object_object_add(ju, "source", json_object_new_string(usage->source_path ? usage->source_path : ""));
                json_object_object_add(ju, "line", json_object_new_int(usage->line));
                json_object_object_add(ju, "col", json_object_new_int(usage->column));
                json_object_array_add(usages_arr, ju);

                if (want_graph && edges) {
                    json_object* edge = json_object_new_object();
                    json_object_object_add(edge, "source", json_object_new_string(usage->source_path ? usage->source_path : ""));
                    json_object_object_add(edge, "target", json_object_new_string(header->name ? header->name : ""));
                    json_object_object_add(edge, "resolved_path", json_object_new_string(header->resolved_path ? header->resolved_path : ""));
                    json_object_object_add(edge, "line", json_object_new_int(usage->line));
                    json_object_object_add(edge, "col", json_object_new_int(usage->column));
                    json_object_object_add(edge, "bucket", json_object_new_string(bucket_kind_name(bucket->kind)));
                    json_object_array_add(edges, edge);
                }
            }

            json_object_object_add(jh, "usages", usages_arr);
            json_object_array_add(headers_arr, jh);
        }

        json_object_object_add(jb, "headers", headers_arr);
        json_object_object_add(jb, "header_count", json_object_new_int((int)header_count));
        json_object_array_add(buckets_arr, jb);
    }
    library_index_unlock();

    json_object_object_add(summary, "headers_total", json_object_new_int(total_headers));
    json_object_object_add(summary, "usages_total", json_object_new_int(total_usages));
    json_object_object_add(summary, "project_headers", json_object_new_int(bucket_headers[LIB_BUCKET_PROJECT]));
    json_object_object_add(summary, "system_headers", json_object_new_int(bucket_headers[LIB_BUCKET_SYSTEM]));
    json_object_object_add(summary, "external_headers", json_object_new_int(bucket_headers[LIB_BUCKET_EXTERNAL]));
    json_object_object_add(summary, "unresolved_headers", json_object_new_int(bucket_headers[LIB_BUCKET_UNRESOLVED]));

    json_object_object_add(result, "summary", summary);
    json_object_object_add(result, "buckets", buckets_arr);
    json_object_object_add(result, "graph", json_object_new_boolean(want_graph));
    if (want_graph && edges) {
        json_object_object_add(result, "edges", edges);
        json_object_object_add(result, "edge_count", json_object_new_int((int)json_object_array_length(edges)));
    }
    return result;
}

typedef struct {
    char* file;
    int line;
    int col;
    char* excerpt;
} SearchMatch;

typedef struct {
    SearchMatch* items;
    size_t count;
    size_t cap;
} SearchMatchList;

static void free_search_matches(SearchMatchList* list) {
    if (!list) return;
    for (size_t i = 0; i < list->count; ++i) {
        free(list->items[i].file);
        free(list->items[i].excerpt);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

static bool append_search_match(SearchMatchList* list,
                                const char* file,
                                int line,
                                int col,
                                const char* excerpt) {
    if (!list || !file || !excerpt) return false;
    if (list->count >= list->cap) {
        size_t new_cap = list->cap ? list->cap * 2 : 64;
        SearchMatch* tmp = (SearchMatch*)realloc(list->items, new_cap * sizeof(SearchMatch));
        if (!tmp) return false;
        list->items = tmp;
        list->cap = new_cap;
    }
    SearchMatch* m = &list->items[list->count++];
    m->file = strdup(file);
    m->line = line;
    m->col = col;
    m->excerpt = strdup(excerpt);
    return (m->file != NULL && m->excerpt != NULL);
}

static int search_match_cmp(const void* a, const void* b) {
    const SearchMatch* ma = (const SearchMatch*)a;
    const SearchMatch* mb = (const SearchMatch*)b;
    if (!ma || !mb) return 0;
    int c = strcmp(ma->file ? ma->file : "", mb->file ? mb->file : "");
    if (c != 0) return c;
    if (ma->line != mb->line) return (ma->line < mb->line) ? -1 : 1;
    if (ma->col != mb->col) return (ma->col < mb->col) ? -1 : 1;
    return 0;
}

static bool should_skip_search_dir(const char* name) {
    return strcmp(name, ".") == 0 ||
           strcmp(name, "..") == 0 ||
           strcmp(name, ".git") == 0 ||
           strcmp(name, "build") == 0 ||
           strcmp(name, "ide_files") == 0;
}

static bool file_is_in_filter_list(const char* path, json_object* files_filter) {
    if (!files_filter || !json_object_is_type(files_filter, json_type_array)) return true;
    size_t n = json_object_array_length(files_filter);
    if (n == 0) return true;
    for (size_t i = 0; i < n; ++i) {
        json_object* item = json_object_array_get_idx(files_filter, i);
        if (!item || !json_object_is_type(item, json_type_string)) continue;
        const char* f = json_object_get_string(item);
        if (!f || !*f) continue;
        if (strcmp(path, f) == 0) return true;
        size_t flen = strlen(f);
        size_t plen = strlen(path);
        if (plen >= flen && strcmp(path + (plen - flen), f) == 0) return true;
    }
    return false;
}

static bool search_file_collect(const char* file_path,
                                const char* pattern,
                                bool regex_mode,
                                regex_t* compiled_re,
                                SearchMatchList* out,
                                int max_items) {
    FILE* f = fopen(file_path, "r");
    if (!f) return true;
    char line[4096];
    int line_no = 0;
    while (fgets(line, sizeof(line), f)) {
        line_no++;
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        if (regex_mode) {
            if (!compiled_re) continue;
            const char* cursor = line;
            int base_col = 1;
            while (*cursor) {
                regmatch_t m;
                int rc = regexec(compiled_re, cursor, 1, &m, 0);
                if (rc != 0) break;
                if (m.rm_so < 0) break;
                int col = base_col + (int)m.rm_so;
                if (!append_search_match(out, file_path, line_no, col, line)) {
                    fclose(f);
                    return false;
                }
                if (max_items >= 0 && (int)out->count >= max_items) {
                    fclose(f);
                    return true;
                }
                int advance = (int)m.rm_eo;
                if (advance <= 0) advance = 1;
                cursor += advance;
                base_col += advance;
            }
        } else {
            const char* cursor = line;
            while (cursor && *cursor) {
                const char* found = strstr(cursor, pattern);
                if (!found) break;
                int col = (int)(found - line) + 1;
                if (!append_search_match(out, file_path, line_no, col, line)) {
                    fclose(f);
                    return false;
                }
                if (max_items >= 0 && (int)out->count >= max_items) {
                    fclose(f);
                    return true;
                }
                cursor = found + 1;
            }
        }
    }
    fclose(f);
    return true;
}

static bool scan_search_dir(const char* root,
                            const char* pattern,
                            bool regex_mode,
                            regex_t* compiled_re,
                            json_object* files_filter,
                            SearchMatchList* out,
                            int max_items) {
    DIR* dir = opendir(root);
    if (!dir) return true;
    struct dirent* ent = NULL;
    char child[PATH_MAX];
    while ((ent = readdir(dir)) != NULL) {
        if (should_skip_search_dir(ent->d_name)) continue;
        snprintf(child, sizeof(child), "%s/%s", root, ent->d_name);
        struct stat st;
        if (stat(child, &st) != 0) continue;
        if (S_ISDIR(st.st_mode)) {
            if (!scan_search_dir(child, pattern, regex_mode, compiled_re, files_filter, out, max_items)) {
                closedir(dir);
                return false;
            }
            if (max_items >= 0 && (int)out->count >= max_items) break;
        } else if (S_ISREG(st.st_mode)) {
            if (!file_is_in_filter_list(child, files_filter)) continue;
            if (!search_file_collect(child, pattern, regex_mode, compiled_re, out, max_items)) {
                closedir(dir);
                return false;
            }
            if (max_items >= 0 && (int)out->count >= max_items) break;
        }
    }
    closedir(dir);
    return true;
}

static json_object* build_search_result(json_object* args, const char* project_root, json_object** error_out) {
    const char* pattern = NULL;
    bool regex_mode = false;
    int max_items = 500;
    json_object* files_filter = NULL;
    *error_out = NULL;

    if (args) {
        json_object *jpat=NULL,*jregex=NULL,*jmax=NULL,*jfiles=NULL;
        if (json_object_object_get_ex(args, "pattern", &jpat) &&
            jpat && json_object_is_type(jpat, json_type_string)) {
            pattern = json_object_get_string(jpat);
        }
        if (json_object_object_get_ex(args, "regex", &jregex) &&
            jregex && json_object_is_type(jregex, json_type_boolean)) {
            regex_mode = json_object_get_boolean(jregex);
        }
        if (json_object_object_get_ex(args, "max", &jmax) &&
            jmax && json_object_is_type(jmax, json_type_int)) {
            max_items = json_object_get_int(jmax);
            if (max_items < 1) max_items = 1;
        }
        if (json_object_object_get_ex(args, "files", &jfiles) &&
            jfiles && json_object_is_type(jfiles, json_type_array)) {
            files_filter = jfiles;
        }
    }

    if (!pattern || !*pattern) {
        *error_out = build_error_obj("bad_request", "search requires args.pattern", NULL);
        return NULL;
    }
    if (!project_root || !*project_root) {
        *error_out = build_error_obj("bad_state", "Project root unavailable", NULL);
        return NULL;
    }

    regex_t re;
    regex_t* re_ptr = NULL;
    if (regex_mode) {
        if (regcomp(&re, pattern, REG_EXTENDED) != 0) {
            *error_out = build_error_obj("bad_regex", "Invalid regex pattern", pattern);
            return NULL;
        }
        re_ptr = &re;
    }

    SearchMatchList matches = {0};
    bool ok = scan_search_dir(project_root, pattern, regex_mode, re_ptr, files_filter, &matches, max_items);
    if (regex_mode) regfree(&re);
    if (!ok) {
        free_search_matches(&matches);
        *error_out = build_error_obj("search_failed", "Failed while scanning project files", NULL);
        return NULL;
    }

    if (matches.count > 1) {
        qsort(matches.items, matches.count, sizeof(SearchMatch), search_match_cmp);
    }

    json_object* result = json_object_new_object();
    json_object* arr = json_object_new_array();
    for (size_t i = 0; i < matches.count; ++i) {
        json_object* m = json_object_new_object();
        json_object_object_add(m, "file", json_object_new_string(matches.items[i].file ? matches.items[i].file : ""));
        json_object_object_add(m, "line", json_object_new_int(matches.items[i].line));
        json_object_object_add(m, "col", json_object_new_int(matches.items[i].col));
        json_object_object_add(m, "excerpt", json_object_new_string(matches.items[i].excerpt ? matches.items[i].excerpt : ""));
        json_object_array_add(arr, m);
    }
    json_object_object_add(result, "pattern", json_object_new_string(pattern));
    json_object_object_add(result, "regex", json_object_new_boolean(regex_mode));
    json_object_object_add(result, "max", json_object_new_int(max_items));
    json_object_object_add(result, "match_count", json_object_new_int((int)matches.count));
    json_object_object_add(result, "matches", arr);

    free_search_matches(&matches);
    return result;
}

static bool has_makefile_in_dir(const char* dir) {
    if (!dir || !*dir) return false;
    struct stat st;
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/Makefile", dir);
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) return true;
    snprintf(path, sizeof(path), "%s/makefile", dir);
    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) return true;
    return false;
}

static bool is_valid_build_profile(const char* profile) {
    if (!profile || !*profile) return true;
    return (strcmp(profile, "debug") == 0 || strcmp(profile, "perf") == 0);
}

typedef struct {
    char* storage;
    char** argv;
    size_t argc;
} ExecCommandTokens;

typedef struct {
    char** items;
    size_t count;
    size_t cap;
} SourceFileList;

static void free_exec_command_tokens(ExecCommandTokens* tokens) {
    if (!tokens) return;
    free(tokens->storage);
    free(tokens->argv);
    memset(tokens, 0, sizeof(*tokens));
}

static bool parse_command_tokens(const char* text,
                                 ExecCommandTokens* out,
                                 char* error_out,
                                 size_t error_out_cap) {
    if (error_out && error_out_cap > 0) error_out[0] = '\0';
    if (!out) return false;
    memset(out, 0, sizeof(*out));
    if (!text || !text[0]) {
        str_copy(error_out, error_out_cap, "Command is empty");
        return false;
    }

    const size_t len = strlen(text);
    out->storage = (char*)malloc(len + 1);
    size_t argv_cap = (len / 2) + 4;
    out->argv = (char**)calloc(argv_cap, sizeof(char*));
    if (!out->storage || !out->argv) {
        free_exec_command_tokens(out);
        str_copy(error_out, error_out_cap, "Failed to allocate command parser state");
        return false;
    }

    const char* p = text;
    char* w = out->storage;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;

        if (out->argc + 1 >= argv_cap) {
            free_exec_command_tokens(out);
            str_copy(error_out, error_out_cap, "Command has too many arguments");
            return false;
        }

        out->argv[out->argc++] = w;
        char quote = '\0';
        while (*p) {
            char c = *p;
            if (quote) {
                if (c == quote) {
                    quote = '\0';
                    p++;
                    continue;
                }
                if (c == '\\' && quote == '"' && p[1]) {
                    *w++ = p[1];
                    p += 2;
                    continue;
                }
                *w++ = c;
                p++;
                continue;
            }

            if (isspace((unsigned char)c)) break;
            if (c == '"' || c == '\'') {
                quote = c;
                p++;
                continue;
            }
            if (c == '\\' && p[1]) {
                *w++ = p[1];
                p += 2;
                continue;
            }

            *w++ = c;
            p++;
        }

        if (quote) {
            free_exec_command_tokens(out);
            str_copy(error_out, error_out_cap, "Unterminated quote in command");
            return false;
        }

        *w++ = '\0';
        while (*p && isspace((unsigned char)*p)) p++;
    }

    if (out->argc == 0) {
        free_exec_command_tokens(out);
        str_copy(error_out, error_out_cap, "Command is empty");
        return false;
    }

    out->argv[out->argc] = NULL;
    return true;
}

static void free_source_file_list(SourceFileList* list) {
    if (!list) return;
    for (size_t i = 0; i < list->count; ++i) {
        free(list->items[i]);
    }
    free(list->items);
    memset(list, 0, sizeof(*list));
}

static bool append_source_file(SourceFileList* list, const char* path) {
    if (!list || !path || !path[0]) return false;
    if (list->count == list->cap) {
        size_t next_cap = list->cap ? (list->cap * 2) : 32;
        char** grown = (char**)realloc(list->items, next_cap * sizeof(char*));
        if (!grown) return false;
        list->items = grown;
        list->cap = next_cap;
    }
    list->items[list->count] = strdup(path);
    if (!list->items[list->count]) return false;
    list->count++;
    return true;
}

static bool should_skip_fallback_entry(const char* name) {
    if (!name || !name[0]) return true;
    return (strcmp(name, ".") == 0 ||
            strcmp(name, "..") == 0 ||
            strcmp(name, "build") == 0 ||
            strcmp(name, ".git") == 0 ||
            strcmp(name, "ide_files") == 0);
}

static bool collect_fallback_sources_recursive(const char* dir, SourceFileList* out) {
    if (!dir || !dir[0] || !out) return false;
    DIR* dp = opendir(dir);
    if (!dp) return false;

    struct dirent* ent = NULL;
    bool ok = true;
    while ((ent = readdir(dp)) != NULL) {
        if (should_skip_fallback_entry(ent->d_name)) continue;

        char path[PATH_MAX];
        if (snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name) >= (int)sizeof(path)) {
            ok = false;
            break;
        }

        struct stat st;
        if (lstat(path, &st) != 0) {
            ok = false;
            break;
        }

        if (S_ISLNK(st.st_mode)) continue;
        if (S_ISDIR(st.st_mode)) {
            if (!collect_fallback_sources_recursive(path, out)) {
                ok = false;
                break;
            }
            continue;
        }

        if (!S_ISREG(st.st_mode)) continue;
        size_t name_len = strlen(ent->d_name);
        if (name_len < 3 || strcmp(ent->d_name + (name_len - 2), ".c") != 0) continue;
        if (!append_source_file(out, path)) {
            ok = false;
            break;
        }
    }

    closedir(dp);
    return ok;
}

static int run_exec_capture(const char* working_dir,
                            char* const argv[],
                            const char* profile,
                            char* output,
                            size_t output_cap,
                            size_t* out_len) {
    if (!working_dir || !*working_dir || !output || output_cap == 0) return -1;
    if (out_len) *out_len = 0;
    output[0] = '\0';
    if (!argv || !argv[0] || !argv[0][0]) return -1;

    int pipe_fd[2];
    if (pipe(pipe_fd) != 0) {
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return -1;
    }

    if (pid == 0) {
        if (chdir(working_dir) != 0) _exit(127);
        if (profile && *profile) {
            setenv("PROFILE", profile, 1);
        } else {
            unsetenv("PROFILE");
        }
        if (dup2(pipe_fd[1], STDOUT_FILENO) < 0) _exit(127);
        if (dup2(pipe_fd[1], STDERR_FILENO) < 0) _exit(127);
        close(pipe_fd[0]);
        close(pipe_fd[1]);

        execvp(argv[0], argv);
        _exit(127);
    }

    close(pipe_fd[1]);
    FILE* pipe_stream = fdopen(pipe_fd[0], "r");
    if (!pipe_stream) {
        close(pipe_fd[0]);
        (void)waitpid(pid, NULL, 0);
        return -1;
    }

    size_t written = 0;
    char line[512];
    while (fgets(line, sizeof(line), pipe_stream)) {
        size_t n = strlen(line);
        if (written + n < output_cap - 1) {
            memcpy(output + written, line, n);
            written += n;
            output[written] = '\0';
        }
        build_diagnostics_feed_chunk(line, n);
    }
    fclose(pipe_stream);
    if (out_len) *out_len = written;

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        if (errno == EINTR) continue;
        return -1;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return 1;
}

static int run_make_capture(const char* working_dir,
                            const char* profile,
                            char* output,
                            size_t output_cap,
                            size_t* out_len) {
    char* const argv[] = { "make", NULL };
    return run_exec_capture(working_dir, argv, profile, output, output_cap, out_len);
}

static int run_command_text_capture(const char* working_dir,
                                    const char* command_text,
                                    const char* profile,
                                    char* output,
                                    size_t output_cap,
                                    size_t* out_len,
                                    char* error_out,
                                    size_t error_out_cap) {
    ExecCommandTokens tokens = {0};
    if (!parse_command_tokens(command_text, &tokens, error_out, error_out_cap)) {
        return -1;
    }
    int status = run_exec_capture(working_dir,
                                  tokens.argv,
                                  profile,
                                  output,
                                  output_cap,
                                  out_len);
    free_exec_command_tokens(&tokens);
    return status;
}

static int run_fallback_compile_capture(const char* project_root,
                                        const char* profile,
                                        char* output,
                                        size_t output_cap,
                                        size_t* out_len) {
    if (!project_root || !project_root[0] || !output || output_cap == 0) return -1;
    if (out_len) *out_len = 0;
    output[0] = '\0';

    char build_dir[PATH_MAX];
    if (snprintf(build_dir, sizeof(build_dir), "%s/build", project_root) >= (int)sizeof(build_dir)) {
        return -1;
    }
    if (!ensure_dir_exists(build_dir, 0755)) {
        str_copy(output, output_cap, "Failed to create build directory\n");
        if (out_len) *out_len = strlen(output);
        build_diagnostics_feed_chunk(output, strlen(output));
        return 1;
    }

    SourceFileList sources = {0};
    if (!collect_fallback_sources_recursive(project_root, &sources)) {
        free_source_file_list(&sources);
        str_copy(output, output_cap, "Failed to enumerate project source files\n");
        if (out_len) *out_len = strlen(output);
        build_diagnostics_feed_chunk(output, strlen(output));
        return 1;
    }

    if (sources.count == 0) {
        free_source_file_list(&sources);
        str_copy(output, output_cap, "No C source files found for fallback build\n");
        if (out_len) *out_len = strlen(output);
        build_diagnostics_feed_chunk(output, strlen(output));
        return 1;
    }

    char** argv = (char**)calloc(sources.count + 9, sizeof(char*));
    if (!argv) {
        free_source_file_list(&sources);
        return -1;
    }

    size_t argc = 0;
    argv[argc++] = "cc";
    argv[argc++] = "-std=c11";
    argv[argc++] = "-Wall";
    argv[argc++] = "-Wextra";
    argv[argc++] = "-g";
    argv[argc++] = "-Iinclude";
    argv[argc++] = "-Isrc";
    argv[argc++] = "-o";
    argv[argc++] = "build/app";
    for (size_t i = 0; i < sources.count; ++i) {
        argv[argc++] = sources.items[i];
    }
    argv[argc] = NULL;

    int status = run_exec_capture(project_root, argv, profile, output, output_cap, out_len);
    free(argv);
    free_source_file_list(&sources);
    return status;
}

static void expand_path_relative(const char* baseDir, const char* path, char* out, size_t outSize) {
    if (!out || outSize == 0) return;
    out[0] = '\0';
    if (!path || !*path) return;
    if (path[0] == '/') {
        strncpy(out, path, outSize - 1);
        out[outSize - 1] = '\0';
        return;
    }
    if (path[0] == '~') {
        const char* home = getenv("HOME");
        if (!home) home = "";
        if (path[1] == '/' || path[1] == '\\') snprintf(out, outSize, "%s/%s", home, path + 2);
        else if (path[1] == '\0') snprintf(out, outSize, "%s", home);
        else snprintf(out, outSize, "%s/%s", home, path + 1);
        return;
    }
    if (baseDir && *baseDir) snprintf(out, outSize, "%s/%s", baseDir, path);
    else snprintf(out, outSize, "%s", path);
}

static json_object* build_build_result(json_object* args, const char* project_root, json_object** error_out) {
    *error_out = NULL;
    if (!project_root || !*project_root) {
        *error_out = build_error_obj("bad_state", "Project root unavailable", NULL);
        return NULL;
    }

    const WorkspaceBuildConfig* cfg = getWorkspaceBuildConfig();
    const char* profile = NULL;
    if (args) {
        json_object* jprofile = NULL;
        if (json_object_object_get_ex(args, "profile", &jprofile) &&
            jprofile && json_object_is_type(jprofile, json_type_string)) {
            profile = json_object_get_string(jprofile);
        }
    }
    if (!is_valid_build_profile(profile)) {
        *error_out = build_error_obj("bad_request", "Invalid profile. Expected debug or perf.", profile);
        return NULL;
    }

    char workingDir[PATH_MAX];
    snprintf(workingDir, sizeof(workingDir), "%s", project_root);
    if (cfg && cfg->build_working_dir[0]) {
        expand_path_relative(project_root, cfg->build_working_dir, workingDir, sizeof(workingDir));
    }

    char command_text[2048];
    command_text[0] = '\0';
    char command_display[2048];
    command_display[0] = '\0';
    bool use_make_exec = false;
    bool use_fallback_exec = false;
    if (cfg && cfg->build_command[0]) {
        snprintf(command_text, sizeof(command_text), "%s%s%s",
                 cfg->build_command,
                 cfg->build_args[0] ? " " : "",
                 cfg->build_args[0] ? cfg->build_args : "");
        snprintf(command_display, sizeof(command_display), "%s%s%s",
                 cfg->build_command,
                 cfg->build_args[0] ? " " : "",
                 cfg->build_args[0] ? cfg->build_args : "");
    } else if (has_makefile_in_dir(project_root)) {
        use_make_exec = true;
        if (profile && *profile) {
            snprintf(command_display, sizeof(command_display), "make (PROFILE=%s)", profile);
        } else {
            snprintf(command_display, sizeof(command_display), "make");
        }
    } else {
        snprintf(command_display, sizeof(command_display),
                 "cc -std=c11 -Wall -Wextra -g -Iinclude -Isrc -o build/app <project sources>");
        use_fallback_exec = true;
    }

    build_diagnostics_clear();

    char output[32768];
    size_t out_len = 0;
    output[0] = '\0';
    int exit_code = 1;

    if (use_make_exec) {
        int make_status = run_make_capture(project_root, profile, output, sizeof(output), &out_len);
        if (make_status < 0) {
            *error_out = build_error_obj("build_failed", "Failed to execute make command", command_display);
            return NULL;
        }
        exit_code = make_status;
    } else if (use_fallback_exec) {
        int fallback_status = run_fallback_compile_capture(project_root, profile, output, sizeof(output), &out_len);
        if (fallback_status < 0) {
            *error_out = build_error_obj("build_failed", "Failed to execute fallback build command", command_display);
            return NULL;
        }
        exit_code = fallback_status;
    } else {
        char parse_err[256] = {0};
        int cmd_status = run_command_text_capture(workingDir,
                                                  command_text,
                                                  profile,
                                                  output,
                                                  sizeof(output),
                                                  &out_len,
                                                  parse_err,
                                                  sizeof(parse_err));
        if (cmd_status < 0) {
            *error_out = build_error_obj("build_failed",
                                         parse_err[0] ? parse_err : "Failed to execute build command",
                                         command_display);
            return NULL;
        }
        exit_code = cmd_status;
    }

    size_t diag_count = 0;
    const BuildDiagnostic* build_diags = build_diagnostics_get(&diag_count);
    int errors = 0;
    int warnings = 0;
    for (size_t i = 0; i < diag_count; ++i) {
        if (build_diags[i].isError) errors++;
        else warnings++;
    }

    json_object* result = json_object_new_object();
    json_object_object_add(result, "ok", json_object_new_boolean(exit_code == 0));
    json_object_object_add(result, "status", json_object_new_string(exit_code == 0 ? "success" : "failed"));
    json_object_object_add(result, "exit_code", json_object_new_int(exit_code));
    json_object_object_add(result, "command", json_object_new_string(command_display));
    json_object_object_add(result, "working_dir", json_object_new_string(workingDir));
    json_object_object_add(result, "profile", json_object_new_string(profile ? profile : ""));
    json_object_object_add(result, "output", json_object_new_string(output));
    json_object_object_add(result, "output_truncated", json_object_new_boolean(out_len >= sizeof(output) - 1));

    json_object* diag_summary = json_object_new_object();
    json_object_object_add(diag_summary, "total", json_object_new_int((int)diag_count));
    json_object_object_add(diag_summary, "error", json_object_new_int(errors));
    json_object_object_add(diag_summary, "warn", json_object_new_int(warnings));
    json_object_object_add(result, "diagnostics_summary", diag_summary);
    return result;
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

static bool verify_edit_hashes(const char* project_root,
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

static json_object* build_error_obj(const char* code, const char* message, const char* details) {
    json_object* err = json_object_new_object();
    json_object_object_add(err, "code", json_object_new_string(code ? code : "error"));
    json_object_object_add(err, "message", json_object_new_string(message ? message : "Unknown error"));
    if (details && *details) {
        json_object_object_add(err, "details", json_object_new_string(details));
    }
    return err;
}

static char* build_response_json(const char* id, bool ok, json_object* result, json_object* error) {
    json_object* root = json_object_new_object();
    json_object_object_add(root, "id", json_object_new_string(id && *id ? id : "unknown"));
    json_object_object_add(root, "ok", json_object_new_boolean(ok));
    if (ok) {
        if (!result) result = json_object_new_object();
        json_object_object_add(root, "result", result);
    } else {
        if (!error) error = build_error_obj("internal_error", "Missing error payload", NULL);
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

static char* handle_request_payload(const char* payload) {
    char req_id[128] = "unknown";

    if (!payload || !*payload) {
        json_object* err = build_error_obj("bad_request", "Request body is empty", NULL);
        return build_response_json(req_id, false, NULL, err);
    }

    json_object* root = json_tokener_parse(payload);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        json_object* err = build_error_obj("bad_json", "Request must be a JSON object", NULL);
        return build_response_json(req_id, false, NULL, err);
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
        json_object* err = build_error_obj("bad_request", "Missing integer field 'proto'", NULL);
        return build_response_json(req_id, false, NULL, err);
    }

    int proto = json_object_get_int(j_proto);
    if (proto != IDE_IPC_PROTO_VERSION) {
        json_object_put(root);
        json_object* err = build_error_obj("bad_proto", "Unsupported protocol version", NULL);
        return build_response_json(req_id, false, NULL, err);
    }

    if (!json_object_object_get_ex(root, "cmd", &j_cmd) || !j_cmd || !json_object_is_type(j_cmd, json_type_string)) {
        json_object_put(root);
        json_object* err = build_error_obj("bad_request", "Missing string field 'cmd'", NULL);
        return build_response_json(req_id, false, NULL, err);
    }

    if (json_object_object_get_ex(root, "args", &j_args) && j_args && !json_object_is_type(j_args, json_type_object)) {
        json_object_put(root);
        json_object* err = build_error_obj("bad_request", "Field 'args' must be an object", NULL);
        return build_response_json(req_id, false, NULL, err);
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
        json_object* err = build_error_obj("auth_required",
                                           "Mutating command requires a valid session auth token",
                                           NULL);
        return build_response_json(req_id, false, NULL, err);
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
        out = build_response_json(req_id, true, result, NULL);
    } else if (strcmp(cmd, "diag") == 0) {
        pthread_mutex_lock(&g_server.lock);
        char project_root[sizeof(g_server.project_root)];
        str_copy(project_root, sizeof(project_root), g_server.project_root);
        pthread_mutex_unlock(&g_server.lock);
        json_object* result = build_diag_result(j_args, project_root);
        out = build_response_json(req_id, true, result, NULL);
    } else if (strcmp(cmd, "symbols") == 0) {
        pthread_mutex_lock(&g_server.lock);
        char project_root[sizeof(g_server.project_root)];
        str_copy(project_root, sizeof(project_root), g_server.project_root);
        pthread_mutex_unlock(&g_server.lock);
        json_object* result = build_symbol_result(j_args, project_root);
        out = build_response_json(req_id, true, result, NULL);
    } else if (strcmp(cmd, "tokens") == 0) {
        pthread_mutex_lock(&g_server.lock);
        char project_root[sizeof(g_server.project_root)];
        str_copy(project_root, sizeof(project_root), g_server.project_root);
        pthread_mutex_unlock(&g_server.lock);
        json_object* result = build_token_result(j_args, project_root);
        out = build_response_json(req_id, true, result, NULL);
    } else if (strcmp(cmd, "includes") == 0) {
        json_object* result = build_includes_result(j_args);
        out = build_response_json(req_id, true, result, NULL);
    } else if (strcmp(cmd, "search") == 0) {
        pthread_mutex_lock(&g_server.lock);
        char project_root[sizeof(g_server.project_root)];
        str_copy(project_root, sizeof(project_root), g_server.project_root);
        pthread_mutex_unlock(&g_server.lock);

        json_object* err = NULL;
        json_object* result = build_search_result(j_args, project_root, &err);
        if (!result) {
            out = build_response_json(req_id, false, NULL, err);
        } else {
            out = build_response_json(req_id, true, result, NULL);
        }
    } else if (strcmp(cmd, "build") == 0) {
        pthread_mutex_lock(&g_server.lock);
        char project_root[sizeof(g_server.project_root)];
        str_copy(project_root, sizeof(project_root), g_server.project_root);
        pthread_mutex_unlock(&g_server.lock);

        json_object* err = NULL;
        json_object* result = build_build_result(j_args, project_root, &err);
        if (!result) {
            out = build_response_json(req_id, false, NULL, err);
        } else {
            out = build_response_json(req_id, true, result, NULL);
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
            json_object* err = build_error_obj("bad_request", "edit requires args.op=\"apply\"", NULL);
            out = build_response_json(req_id, false, NULL, err);
            json_object_put(root);
            return out;
        }
        if (j_args && json_object_object_get_ex(j_args, "diff", &jdiff) &&
            jdiff && json_object_is_type(jdiff, json_type_string)) {
            diff_text = json_object_get_string(jdiff);
        }
        if (!diff_text || !*diff_text) {
            json_object* err = build_error_obj("bad_request", "edit apply requires args.diff", NULL);
            out = build_response_json(req_id, false, NULL, err);
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
        if (!verify_edit_hashes(project_root, diff_text, check_hash, jhashes, hash_err, sizeof(hash_err))) {
            json_object* err = build_error_obj("hash_mismatch", hash_err[0] ? hash_err : "Hash verification failed", NULL);
            out = build_response_json(req_id, false, NULL, err);
            json_object_put(root);
            return out;
        }

        pthread_mutex_lock(&g_server.edit_lock);
        if (!g_server.edit_handler) {
            pthread_mutex_unlock(&g_server.edit_lock);
            json_object* err = build_error_obj("edit_unavailable", "IDE edit handler is not configured", NULL);
            out = build_response_json(req_id, false, NULL, err);
            json_object_put(root);
            return out;
        }
        if (g_server.edit_inflight) {
            pthread_mutex_unlock(&g_server.edit_lock);
            json_object* err = build_error_obj("edit_busy", "Another edit apply request is in flight", NULL);
            out = build_response_json(req_id, false, NULL, err);
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
            out = build_response_json(req_id, true, result, NULL);
        } else {
            json_object* err = build_error_obj("edit_failed",
                                               edit_error[0] ? edit_error : "Failed to apply edit patch",
                                               NULL);
            out = build_response_json(req_id, false, NULL, err);
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
            json_object* err = build_error_obj("bad_request",
                                               "open requires args.path (string), args.line (int), args.col (int)",
                                               NULL);
            return build_response_json(req_id, false, NULL, err);
        }

        const char* req_path = json_object_get_string(jpath);
        int req_line = json_object_get_int(jline);
        int req_col = json_object_get_int(jcol);
        if (!req_path || !req_path[0]) {
            json_object_put(root);
            json_object* err = build_error_obj("bad_request", "open path is empty", NULL);
            return build_response_json(req_id, false, NULL, err);
        }

        pthread_mutex_lock(&g_server.open_lock);
        if (!g_server.open_handler) {
            pthread_mutex_unlock(&g_server.open_lock);
            json_object* err = build_error_obj("open_unavailable",
                                               "IDE open handler is not configured",
                                               NULL);
            out = build_response_json(req_id, false, NULL, err);
        } else if (g_server.open_inflight) {
            pthread_mutex_unlock(&g_server.open_lock);
            json_object* err = build_error_obj("open_busy",
                                               "Another open request is in flight",
                                               NULL);
            out = build_response_json(req_id, false, NULL, err);
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
                out = build_response_json(req_id, true, result, NULL);
            } else {
                json_object* err = build_error_obj("open_failed",
                                                   open_error[0] ? open_error : "Failed to apply open request",
                                                   NULL);
                out = build_response_json(req_id, false, NULL, err);
            }
        }
    } else {
        json_object* err = build_error_obj("unknown_cmd", "Unknown command", cmd);
        out = build_response_json(req_id, false, NULL, err);
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
    if (!build_socket_path(socket_path, sizeof(socket_path), session_id, sizeof(session_id))) {
        fprintf(stderr, "[IPC] Failed to prepare socket path.\n");
        return false;
    }
    if (!generate_auth_token_hex(auth_token)) {
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
