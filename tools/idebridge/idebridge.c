#include <errno.h>
#include <json-c/json.h>
#include <stdbool.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include "core/Diagnostics/diagnostics_pack_export.h"
#include "core/Diagnostics/diagnostics_core_data_export.h"

#define IDEBRIDGE_PROTO 1

enum {
    IDEBRIDGE_EXIT_OK = 0,
    IDEBRIDGE_EXIT_USAGE = 2,
    IDEBRIDGE_EXIT_CONNECT = 3,
    IDEBRIDGE_EXIT_TIMEOUT = 4,
    IDEBRIDGE_EXIT_PROTOCOL = 5,
    IDEBRIDGE_EXIT_SERVER = 6,
};

typedef struct {
    const char* socket_override;
    const char* token_override;
    int timeout_ms;
    const char* spill_file;
} CliGlobalOptions;

static const char* resolve_socket_path(const CliGlobalOptions* opts) {
    if (opts && opts->socket_override && opts->socket_override[0]) {
        return opts->socket_override;
    }
    return getenv("MYIDE_SOCKET");
}

static const char* resolve_auth_token(const CliGlobalOptions* opts) {
    if (opts && opts->token_override && opts->token_override[0]) {
        return opts->token_override;
    }
    return getenv("MYIDE_AUTH_TOKEN");
}

static bool write_text_file(const char* path, const char* text) {
    if (!path || !*path || !text) return false;
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    size_t len = strlen(text);
    bool ok = (fwrite(text, 1, len, f) == len);
    if (ok) ok = (fwrite("\n", 1, 1, f) == 1);
    fclose(f);
    return ok;
}

static void print_usage(FILE* out) {
    fprintf(out,
            "Usage:\n"
            "  idebridge [global_options] ping [--json]\n"
            "  idebridge [global_options] diag [--json] [--max N]\n"
            "  idebridge [global_options] diag-pack [--out path] [--max N]\n"
            "  idebridge [global_options] diag-dataset [--out path] [--max N]\n"
            "  idebridge [global_options] symbols [--json] [--file path] [--top_level_only] [--max N]\n"
            "  idebridge [global_options] open <path:line:col> [--json]\n"
            "  idebridge [global_options] includes [--json] [--graph]\n"
            "  idebridge [global_options] search <pattern> [--json] [--regex] [--max N] [--files path1,path2,...]\n"
            "  idebridge [global_options] build [--json] [--profile debug|perf]\n"
            "  idebridge [global_options] edit --apply <diff_file> [--no_hash_check] [--json]\n\n"
            "Global options:\n"
            "  --socket <path>      override IDE socket path (instead of MYIDE_SOCKET)\n"
            "  --token <token>      override IPC auth token (instead of MYIDE_AUTH_TOKEN)\n"
            "  --timeout_ms <N>     IPC timeout in milliseconds (default: 4000)\n"
            "  --spill_file <path>  write raw JSON response to file\n\n"
            "Exit codes:\n"
            "  0 ok, 2 usage, 3 connect, 4 timeout, 5 protocol, 6 server_error\n\n"
            "Environment:\n"
            "  MYIDE_SOCKET        Unix socket path to IDE session\n"
            "  MYIDE_AUTH_TOKEN    IPC auth token for mutating commands\n"
            "  MYIDE_PROJECT_ROOT  project path (set by IDE PTY)\n");
}

static int connect_socket(const char* socket_path, int timeout_ms, bool* timed_out_out) {
    if (timed_out_out) *timed_out_out = false;
    if (!socket_path || !*socket_path) return -1;
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path);

    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    int rc = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
    if (rc != 0) {
        if (errno != EINPROGRESS) {
            close(fd);
            return -1;
        }
        fd_set wfds;
        FD_ZERO(&wfds);
        FD_SET(fd, &wfds);
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        rc = select(fd + 1, NULL, &wfds, NULL, &tv);
        if (rc == 0) {
            if (timed_out_out) *timed_out_out = true;
            close(fd);
            return -1;
        }
        if (rc < 0) {
            close(fd);
            return -1;
        }
        int so_error = 0;
        socklen_t slen = sizeof(so_error);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &so_error, &slen) != 0 || so_error != 0) {
            close(fd);
            return -1;
        }
    }

    if (flags >= 0) fcntl(fd, F_SETFL, flags);
    struct timeval timeout = {
        .tv_sec = timeout_ms / 1000,
        .tv_usec = (timeout_ms % 1000) * 1000,
    };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    return fd;
}

static char* read_response(int fd, bool* timed_out_out) {
    if (timed_out_out) *timed_out_out = false;
    size_t cap = 4096;
    size_t len = 0;
    char* buf = (char*)malloc(cap);
    if (!buf) return NULL;

    for (;;) {
        if (len + 1024 >= cap) {
            size_t next = cap * 2;
            char* grown = (char*)realloc(buf, next);
            if (!grown) {
                free(buf);
                return NULL;
            }
            buf = grown;
            cap = next;
        }

        ssize_t n = read(fd, buf + len, cap - len - 1);
        if (n > 0) {
            len += (size_t)n;
            continue;
        }
        if (n == 0) break;
        if (errno == EINTR) continue;
        if ((errno == EAGAIN || errno == EWOULDBLOCK) && timed_out_out) {
            *timed_out_out = true;
        }
        free(buf);
        return NULL;
    }

    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) len--;
    buf[len] = '\0';
    return buf;
}

static int parse_target_spec(const char* spec, char* path_out, size_t path_cap, int* line_out, int* col_out) {
    if (!spec || !*spec || !path_out || path_cap == 0 || !line_out || !col_out) return -1;

    const char* last = strrchr(spec, ':');
    if (!last || !*(last + 1)) return -1;
    const char* second_last = last - 1;
    while (second_last >= spec && *second_last != ':') second_last--;
    if (second_last < spec || !*(second_last + 1)) return -1;

    char line_buf[32], col_buf[32];
    size_t path_len = (size_t)(second_last - spec);
    size_t line_len = (size_t)(last - second_last - 1);
    size_t col_len = strlen(last + 1);
    if (path_len == 0 || path_len + 1 > path_cap || line_len == 0 || line_len >= sizeof(line_buf) || col_len == 0 || col_len >= sizeof(col_buf)) {
        return -1;
    }

    memcpy(path_out, spec, path_len);
    path_out[path_len] = '\0';
    memcpy(line_buf, second_last + 1, line_len);
    line_buf[line_len] = '\0';
    memcpy(col_buf, last + 1, col_len);
    col_buf[col_len] = '\0';

    *line_out = atoi(line_buf);
    *col_out = atoi(col_buf);
    if (*line_out <= 0) *line_out = 1;
    if (*col_out <= 0) *col_out = 1;
    return 0;
}

static int send_request(const char* cmd,
                        json_object* args,
                        const CliGlobalOptions* opts,
                        char** response_text_out,
                        json_object** response_obj_out) {
    const char* socket_path = resolve_socket_path(opts);
    if (!socket_path || !*socket_path) {
        fprintf(stderr, "idebridge: no IDE session detected (use --socket or set MYIDE_SOCKET)\n");
        return IDEBRIDGE_EXIT_CONNECT;
    }

    int timeout_ms = (opts && opts->timeout_ms > 0) ? opts->timeout_ms : 4000;
    bool connect_timed_out = false;
    int fd = connect_socket(socket_path, timeout_ms, &connect_timed_out);
    if (fd < 0) {
        if (connect_timed_out) {
            fprintf(stderr, "idebridge: timed out connecting to IDE socket: %s\n", socket_path);
            return IDEBRIDGE_EXIT_TIMEOUT;
        }
        fprintf(stderr, "idebridge: failed to connect to IDE socket: %s\n", socket_path);
        return IDEBRIDGE_EXIT_CONNECT;
    }

    static int req_counter = 1;
    char req_id[64];
    snprintf(req_id, sizeof(req_id), "req-%d", req_counter++);

    json_object* req = json_object_new_object();
    json_object_object_add(req, "id", json_object_new_string(req_id));
    json_object_object_add(req, "proto", json_object_new_int(IDEBRIDGE_PROTO));
    json_object_object_add(req, "cmd", json_object_new_string(cmd));
    const char* auth_token = resolve_auth_token(opts);
    if (auth_token && auth_token[0]) {
        json_object_object_add(req, "auth_token", json_object_new_string(auth_token));
    }
    if (!args) args = json_object_new_object();
    json_object_object_add(req, "args", args);

    const char* req_text = json_object_to_json_string_ext(req, JSON_C_TO_STRING_PLAIN);
    size_t req_len = strlen(req_text);

    bool send_ok = true;
    if (write(fd, req_text, req_len) != (ssize_t)req_len || write(fd, "\n", 1) != 1) {
        send_ok = false;
    }
    json_object_put(req);

    if (!send_ok) {
        close(fd);
        fprintf(stderr, "idebridge: failed to write request (socket closed or timeout)\n");
        return IDEBRIDGE_EXIT_TIMEOUT;
    }

    bool read_timed_out = false;
    char* resp_text = read_response(fd, &read_timed_out);
    close(fd);
    if (!resp_text || !*resp_text) {
        free(resp_text);
        if (read_timed_out) {
            fprintf(stderr, "idebridge: timed out waiting for IDE response\n");
            return IDEBRIDGE_EXIT_TIMEOUT;
        }
        fprintf(stderr, "idebridge: empty response from IDE\n");
        return IDEBRIDGE_EXIT_PROTOCOL;
    }

    if (opts && opts->spill_file && opts->spill_file[0]) {
        if (!write_text_file(opts->spill_file, resp_text)) {
            free(resp_text);
            fprintf(stderr, "idebridge: failed to write spill file: %s\n", opts->spill_file);
            return IDEBRIDGE_EXIT_PROTOCOL;
        }
    }

    json_object* root = json_tokener_parse(resp_text);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        free(resp_text);
        fprintf(stderr, "idebridge: invalid response JSON\n");
        return IDEBRIDGE_EXIT_PROTOCOL;
    }

    *response_text_out = resp_text;
    *response_obj_out = root;
    return IDEBRIDGE_EXIT_OK;
}

static int print_server_error(json_object* root, bool json_output, const char* response_text) {
    if (json_output) {
        printf("%s\n", response_text);
        return IDEBRIDGE_EXIT_SERVER;
    }

    const char* msg = "Unknown server error";
    json_object* jerr = NULL;
    json_object* jmsg = NULL;
    if (json_object_object_get_ex(root, "error", &jerr) && jerr &&
        json_object_object_get_ex(jerr, "message", &jmsg) && jmsg) {
        msg = json_object_get_string(jmsg);
    }
    fprintf(stderr, "idebridge: %s\n", msg ? msg : "Server error");
    return IDEBRIDGE_EXIT_SERVER;
}

static int print_or_validate_response(bool json_output,
                                      char* response_text,
                                      json_object* root,
                                      json_object** result_out) {
    json_object* j_ok = NULL;
    if (!json_object_object_get_ex(root, "ok", &j_ok)) {
        json_object_put(root);
        free(response_text);
        fprintf(stderr, "idebridge: malformed response\n");
        return IDEBRIDGE_EXIT_PROTOCOL;
    }

    if (!json_object_get_boolean(j_ok)) {
        int rc = print_server_error(root, json_output, response_text);
        json_object_put(root);
        free(response_text);
        return rc;
    }

    json_object* result = NULL;
    json_object_object_get_ex(root, "result", &result);
    *result_out = result;

    if (json_output) {
        printf("%s\n", response_text);
        json_object_put(root);
        free(response_text);
        return IDEBRIDGE_EXIT_OK;
    }

    return IDEBRIDGE_EXIT_OK;
}

static int run_ping(bool json_output, const CliGlobalOptions* opts) {
    char* response_text = NULL;
    json_object* root = NULL;
    int rc = send_request("ping", json_object_new_object(), opts, &response_text, &root);
    if (rc != IDEBRIDGE_EXIT_OK) return rc;

    json_object* result = NULL;
    rc = print_or_validate_response(json_output, response_text, root, &result);
    if (rc != IDEBRIDGE_EXIT_OK || json_output) return rc;

    const char* session_id = "";
    const char* project_root = "";
    const char* server_version = "";
    int proto = 0;
    long long uptime_ms = 0;
    if (result) {
        json_object* j = NULL;
        if (json_object_object_get_ex(result, "session_id", &j) && j) session_id = json_object_get_string(j);
        if (json_object_object_get_ex(result, "project_root", &j) && j) project_root = json_object_get_string(j);
        if (json_object_object_get_ex(result, "server_version", &j) && j) server_version = json_object_get_string(j);
        if (json_object_object_get_ex(result, "proto", &j) && j) proto = json_object_get_int(j);
        if (json_object_object_get_ex(result, "uptime_ms", &j) && j) uptime_ms = json_object_get_int64(j);
    }

    printf("idebridge ping ok\n");
    printf("proto: %d\n", proto);
    printf("session: %s\n", session_id ? session_id : "");
    printf("project: %s\n", project_root ? project_root : "");
    printf("server: %s\n", server_version ? server_version : "");
    printf("uptime_ms: %lld\n", uptime_ms);

    json_object_put(root);
    free(response_text);
    return IDEBRIDGE_EXIT_OK;
}

static int run_diag(bool json_output, int max_items, const CliGlobalOptions* opts) {
    json_object* args = json_object_new_object();
    if (max_items >= 0) json_object_object_add(args, "max", json_object_new_int(max_items));

    char* response_text = NULL;
    json_object* root = NULL;
    int rc = send_request("diag", args, opts, &response_text, &root);
    if (rc != IDEBRIDGE_EXIT_OK) return rc;

    json_object* result = NULL;
    rc = print_or_validate_response(json_output, response_text, root, &result);
    if (rc != IDEBRIDGE_EXIT_OK || json_output) return rc;

    json_object *summary=NULL,*arr=NULL,*j=NULL;
    int total = 0, err = 0, warn = 0, info = 0, returned = 0;
    if (result && json_object_object_get_ex(result, "summary", &summary) && summary) {
        if (json_object_object_get_ex(summary, "total", &j) && j) total = json_object_get_int(j);
        if (json_object_object_get_ex(summary, "error", &j) && j) err = json_object_get_int(j);
        if (json_object_object_get_ex(summary, "warn", &j) && j) warn = json_object_get_int(j);
        if (json_object_object_get_ex(summary, "info", &j) && j) info = json_object_get_int(j);
    }
    if (result && json_object_object_get_ex(result, "returned_count", &j) && j) returned = json_object_get_int(j);
    if (result) json_object_object_get_ex(result, "diagnostics", &arr);

    printf("diagnostics: total=%d error=%d warn=%d info=%d returned=%d\n", total, err, warn, info, returned);
    if (arr && json_object_is_type(arr, json_type_array)) {
        size_t n = json_object_array_length(arr);
        for (size_t i = 0; i < n; ++i) {
            json_object* d = json_object_array_get_idx(arr, i);
            if (!d) continue;
            json_object *jfile=NULL,*jline=NULL,*jcol=NULL,*jsev=NULL,*jmsg=NULL;
            json_object_object_get_ex(d, "file", &jfile);
            json_object_object_get_ex(d, "line", &jline);
            json_object_object_get_ex(d, "col", &jcol);
            json_object_object_get_ex(d, "severity", &jsev);
            json_object_object_get_ex(d, "message", &jmsg);
            printf("%s:%d:%d: %s: %s\n",
                   jfile ? json_object_get_string(jfile) : "",
                   jline ? json_object_get_int(jline) : 0,
                   jcol ? json_object_get_int(jcol) : 0,
                   jsev ? json_object_get_string(jsev) : "",
                   jmsg ? json_object_get_string(jmsg) : "");
        }
    }

    json_object_put(root);
    free(response_text);
    return IDEBRIDGE_EXIT_OK;
}

static int run_diag_pack(const char* out_path, int max_items, const CliGlobalOptions* opts) {
    char default_path[1024];
    const char* pack_path = out_path;
    if (!pack_path || !*pack_path) {
        const char* root = getenv("MYIDE_PROJECT_ROOT");
        if (root && *root) {
            snprintf(default_path, sizeof(default_path), "%s/ide_files/diagnostics_snapshot.pack", root);
        } else {
            snprintf(default_path, sizeof(default_path), "diagnostics_snapshot.pack");
        }
        pack_path = default_path;
    }

    json_object* args = json_object_new_object();
    if (max_items >= 0) json_object_object_add(args, "max", json_object_new_int(max_items));

    char* response_text = NULL;
    json_object* root = NULL;
    int rc = send_request("diag", args, opts, &response_text, &root);
    if (rc != IDEBRIDGE_EXIT_OK) return rc;

    json_object* result = NULL;
    rc = print_or_validate_response(false, response_text, root, &result);
    if (rc != IDEBRIDGE_EXIT_OK) return rc;

    json_object* summary = NULL;
    json_object* j = NULL;
    int total = 0;
    int err = 0;
    int warn = 0;
    int info = 0;
    int returned = 0;
    if (result && json_object_object_get_ex(result, "summary", &summary) && summary) {
        if (json_object_object_get_ex(summary, "total", &j) && j) total = json_object_get_int(j);
        if (json_object_object_get_ex(summary, "error", &j) && j) err = json_object_get_int(j);
        if (json_object_object_get_ex(summary, "warn", &j) && j) warn = json_object_get_int(j);
        if (json_object_object_get_ex(summary, "info", &j) && j) info = json_object_get_int(j);
    }
    if (result && json_object_object_get_ex(result, "returned_count", &j) && j) {
        returned = json_object_get_int(j);
    }

    bool ok = diagnostics_pack_export_from_diag_response_json(pack_path, response_text);
    json_object_put(root);
    free(response_text);
    if (!ok) {
        fprintf(stderr, "idebridge: failed to export diagnostics pack: %s\n", pack_path);
        return IDEBRIDGE_EXIT_SERVER;
    }

    printf("diag-pack: wrote %s\n", pack_path);
    printf("diagnostics: total=%d error=%d warn=%d info=%d returned=%d\n", total, err, warn, info, returned);
    return IDEBRIDGE_EXIT_OK;
}

static int run_diag_dataset(const char* out_path, int max_items, const CliGlobalOptions* opts) {
    char default_path[1024];
    const char* dataset_path = out_path;
    if (!dataset_path || !*dataset_path) {
        const char* root = getenv("MYIDE_PROJECT_ROOT");
        if (root && *root) {
            snprintf(default_path, sizeof(default_path), "%s/ide_files/diagnostics_snapshot.dataset.json", root);
        } else {
            snprintf(default_path, sizeof(default_path), "diagnostics_snapshot.dataset.json");
        }
        dataset_path = default_path;
    }

    json_object* args = json_object_new_object();
    if (max_items >= 0) json_object_object_add(args, "max", json_object_new_int(max_items));

    char* response_text = NULL;
    json_object* root = NULL;
    int rc = send_request("diag", args, opts, &response_text, &root);
    if (rc != IDEBRIDGE_EXIT_OK) return rc;

    json_object* result = NULL;
    rc = print_or_validate_response(false, response_text, root, &result);
    if (rc != IDEBRIDGE_EXIT_OK) return rc;

    json_object* summary = NULL;
    json_object* j = NULL;
    int total = 0;
    int err = 0;
    int warn = 0;
    int info = 0;
    int returned = 0;
    if (result && json_object_object_get_ex(result, "summary", &summary) && summary) {
        if (json_object_object_get_ex(summary, "total", &j) && j) total = json_object_get_int(j);
        if (json_object_object_get_ex(summary, "error", &j) && j) err = json_object_get_int(j);
        if (json_object_object_get_ex(summary, "warn", &j) && j) warn = json_object_get_int(j);
        if (json_object_object_get_ex(summary, "info", &j) && j) info = json_object_get_int(j);
    }
    if (result && json_object_object_get_ex(result, "returned_count", &j) && j) {
        returned = json_object_get_int(j);
    }

    bool ok = diagnostics_core_data_export_from_diag_response_json(dataset_path, response_text);
    json_object_put(root);
    free(response_text);
    if (!ok) {
        fprintf(stderr, "idebridge: failed to export diagnostics dataset: %s\n", dataset_path);
        return IDEBRIDGE_EXIT_SERVER;
    }

    printf("diag-dataset: wrote %s\n", dataset_path);
    printf("diagnostics: total=%d error=%d warn=%d info=%d returned=%d\n", total, err, warn, info, returned);
    return IDEBRIDGE_EXIT_OK;
}

static int run_symbols(bool json_output, const char* file_filter, bool top_level_only, int max_items, const CliGlobalOptions* opts) {
    json_object* args = json_object_new_object();
    if (file_filter && *file_filter) json_object_object_add(args, "file", json_object_new_string(file_filter));
    if (top_level_only) json_object_object_add(args, "top_level_only", json_object_new_boolean(true));
    if (max_items >= 0) json_object_object_add(args, "max", json_object_new_int(max_items));

    char* response_text = NULL;
    json_object* root = NULL;
    int rc = send_request("symbols", args, opts, &response_text, &root);
    if (rc != IDEBRIDGE_EXIT_OK) return rc;

    json_object* result = NULL;
    rc = print_or_validate_response(json_output, response_text, root, &result);
    if (rc != IDEBRIDGE_EXIT_OK || json_output) return rc;

    json_object *arr=NULL,*j=NULL;
    int total = 0, returned = 0;
    if (result && json_object_object_get_ex(result, "total_count", &j) && j) total = json_object_get_int(j);
    if (result && json_object_object_get_ex(result, "returned_count", &j) && j) returned = json_object_get_int(j);
    if (result) json_object_object_get_ex(result, "symbols", &arr);

    printf("symbols: total=%d returned=%d\n", total, returned);
    if (arr && json_object_is_type(arr, json_type_array)) {
        size_t n = json_object_array_length(arr);
        for (size_t i = 0; i < n; ++i) {
            json_object* s = json_object_array_get_idx(arr, i);
            if (!s) continue;
            json_object *jkind=NULL,*jname=NULL,*jfile=NULL,*jline=NULL,*jcol=NULL;
            json_object_object_get_ex(s, "kind", &jkind);
            json_object_object_get_ex(s, "name", &jname);
            json_object_object_get_ex(s, "file", &jfile);
            json_object_object_get_ex(s, "start_line", &jline);
            json_object_object_get_ex(s, "start_col", &jcol);
            printf("%s %s @ %s:%d:%d\n",
                   jkind ? json_object_get_string(jkind) : "",
                   jname ? json_object_get_string(jname) : "",
                   jfile ? json_object_get_string(jfile) : "",
                   jline ? json_object_get_int(jline) : 0,
                   jcol ? json_object_get_int(jcol) : 0);
        }
    }

    json_object_put(root);
    free(response_text);
    return IDEBRIDGE_EXIT_OK;
}

static int run_open(bool json_output, const char* target_spec, const CliGlobalOptions* opts) {
    char path[1024];
    int line = 1, col = 1;
    if (parse_target_spec(target_spec, path, sizeof(path), &line, &col) != 0) {
        fprintf(stderr, "idebridge: open target must be path:line:col\n");
        return IDEBRIDGE_EXIT_USAGE;
    }

    json_object* args = json_object_new_object();
    json_object_object_add(args, "path", json_object_new_string(path));
    json_object_object_add(args, "line", json_object_new_int(line));
    json_object_object_add(args, "col", json_object_new_int(col));

    char* response_text = NULL;
    json_object* root = NULL;
    int rc = send_request("open", args, opts, &response_text, &root);
    if (rc != IDEBRIDGE_EXIT_OK) return rc;

    json_object* result = NULL;
    rc = print_or_validate_response(json_output, response_text, root, &result);
    if (rc != IDEBRIDGE_EXIT_OK || json_output) return rc;

    (void)result;
    printf("open applied: %s:%d:%d\n", path, line, col);
    json_object_put(root);
    free(response_text);
    return IDEBRIDGE_EXIT_OK;
}

static int run_includes(bool json_output, bool graph, const CliGlobalOptions* opts) {
    json_object* args = json_object_new_object();
    if (graph) json_object_object_add(args, "graph", json_object_new_boolean(true));

    char* response_text = NULL;
    json_object* root = NULL;
    int rc = send_request("includes", args, opts, &response_text, &root);
    if (rc != IDEBRIDGE_EXIT_OK) return rc;

    json_object* result = NULL;
    rc = print_or_validate_response(json_output, response_text, root, &result);
    if (rc != IDEBRIDGE_EXIT_OK || json_output) return rc;

    json_object *summary=NULL,*j=NULL;
    if (result && json_object_object_get_ex(result, "summary", &summary) && summary) {
        int headers = 0, usages = 0;
        if (json_object_object_get_ex(summary, "headers_total", &j) && j) headers = json_object_get_int(j);
        if (json_object_object_get_ex(summary, "usages_total", &j) && j) usages = json_object_get_int(j);
        printf("includes: headers=%d usages=%d graph=%s\n", headers, usages, graph ? "true" : "false");
    } else {
        printf("includes: ok\n");
    }

    json_object_put(root);
    free(response_text);
    return IDEBRIDGE_EXIT_OK;
}

static void parse_files_csv(const char* csv, json_object* files_arr) {
    if (!csv || !*csv || !files_arr) return;
    char buf[2048];
    snprintf(buf, sizeof(buf), "%s", csv);
    char* save = NULL;
    for (char* tok = strtok_r(buf, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        if (*tok) json_object_array_add(files_arr, json_object_new_string(tok));
    }
}

static bool read_file_text(const char* path, char** out_text) {
    if (!path || !*path || !out_text) return false;
    *out_text = NULL;
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 0 || len > (64 * 1024 * 1024)) {
        fclose(f);
        return false;
    }
    char* buf = (char*)malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        return false;
    }
    size_t n = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[n] = '\0';
    *out_text = buf;
    return true;
}

static unsigned long long fnv1a64_file(const char* path, bool* ok_out) {
    unsigned long long hash = 1469598103934665603ULL;
    if (ok_out) *ok_out = false;
    FILE* f = fopen(path, "rb");
    if (!f) return hash;
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
    if (ok_out) *ok_out = true;
    return hash;
}

static int collect_diff_paths(const char* diff_text, char paths[][1024], int max_paths) {
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
            if (strcmp(paths[k], path) == 0) { exists = true; break; }
        }
        if (!exists && count < max_paths) {
            snprintf(paths[count], 1024, "%s", path);
            count++;
        }
    }
    free(tmp);
    return count;
}

static int run_edit_apply(bool json_output, const char* diff_file, bool no_hash_check, const CliGlobalOptions* opts) {
    if (!diff_file || !*diff_file) {
        fprintf(stderr, "idebridge: edit --apply requires a diff file path\n");
        return IDEBRIDGE_EXIT_USAGE;
    }

    char* diff_text = NULL;
    if (!read_file_text(diff_file, &diff_text)) {
        fprintf(stderr, "idebridge: failed to read diff file: %s\n", diff_file);
        return IDEBRIDGE_EXIT_USAGE;
    }

    json_object* args = json_object_new_object();
    json_object_object_add(args, "op", json_object_new_string("apply"));
    json_object_object_add(args, "diff", json_object_new_string(diff_text));
    json_object_object_add(args, "check_hash", json_object_new_boolean(!no_hash_check));

    json_object* hashes = json_object_new_object();
    if (!no_hash_check) {
        const char* root = getenv("MYIDE_PROJECT_ROOT");
        char paths[128][1024];
        int n = collect_diff_paths(diff_text, paths, 128);
        for (int i = 0; i < n; ++i) {
            char abs[1024];
            if (paths[i][0] == '/') snprintf(abs, sizeof(abs), "%s", paths[i]);
            else if (root && *root) snprintf(abs, sizeof(abs), "%s/%s", root, paths[i]);
            else snprintf(abs, sizeof(abs), "%s", paths[i]);

            bool ok_hash = false;
            unsigned long long h = fnv1a64_file(abs, &ok_hash);
            if (!ok_hash) {
                free(diff_text);
                json_object_put(args);
                fprintf(stderr, "idebridge: failed to hash target file: %s\n", abs);
                return IDEBRIDGE_EXIT_USAGE;
            }
            char hex[32];
            snprintf(hex, sizeof(hex), "%016llx", h);
            json_object_object_add(hashes, paths[i], json_object_new_string(hex));
        }
    }
    json_object_object_add(args, "hashes", hashes);
    free(diff_text);

    char* response_text = NULL;
    json_object* root = NULL;
    int rc = send_request("edit", args, opts, &response_text, &root);
    if (rc != IDEBRIDGE_EXIT_OK) return rc;

    json_object* result = NULL;
    rc = print_or_validate_response(json_output, response_text, root, &result);
    if (rc != IDEBRIDGE_EXIT_OK || json_output) return rc;

    json_object* touched = NULL;
    size_t touched_count = 0;
    if (result && json_object_object_get_ex(result, "touched_files", &touched) &&
        touched && json_object_is_type(touched, json_type_array)) {
        touched_count = json_object_array_length(touched);
    }
    printf("edit apply: touched_files=%zu hash_check=%s\n", touched_count, no_hash_check ? "false" : "true");

    json_object_put(root);
    free(response_text);
    return IDEBRIDGE_EXIT_OK;
}

static int run_search(bool json_output,
                      const char* pattern,
                      bool regex_mode,
                      int max_items,
                      const char* files_csv,
                      const CliGlobalOptions* opts) {
    if (!pattern || !*pattern) {
        fprintf(stderr, "idebridge: search requires <pattern>\n");
        return IDEBRIDGE_EXIT_USAGE;
    }

    json_object* args = json_object_new_object();
    json_object_object_add(args, "pattern", json_object_new_string(pattern));
    if (regex_mode) json_object_object_add(args, "regex", json_object_new_boolean(true));
    if (max_items >= 0) json_object_object_add(args, "max", json_object_new_int(max_items));
    if (files_csv && *files_csv) {
        json_object* files = json_object_new_array();
        parse_files_csv(files_csv, files);
        json_object_object_add(args, "files", files);
    }

    char* response_text = NULL;
    json_object* root = NULL;
    int rc = send_request("search", args, opts, &response_text, &root);
    if (rc != IDEBRIDGE_EXIT_OK) return rc;

    json_object* result = NULL;
    rc = print_or_validate_response(json_output, response_text, root, &result);
    if (rc != IDEBRIDGE_EXIT_OK || json_output) return rc;

    json_object *j=NULL,*arr=NULL;
    int count = 0;
    if (result && json_object_object_get_ex(result, "match_count", &j) && j) count = json_object_get_int(j);
    if (result) json_object_object_get_ex(result, "matches", &arr);

    printf("search: matches=%d regex=%s\n", count, regex_mode ? "true" : "false");
    if (arr && json_object_is_type(arr, json_type_array)) {
        size_t n = json_object_array_length(arr);
        for (size_t i = 0; i < n; ++i) {
            json_object* m = json_object_array_get_idx(arr, i);
            if (!m) continue;
            json_object *jf=NULL,*jl=NULL,*jc=NULL,*je=NULL;
            json_object_object_get_ex(m, "file", &jf);
            json_object_object_get_ex(m, "line", &jl);
            json_object_object_get_ex(m, "col", &jc);
            json_object_object_get_ex(m, "excerpt", &je);
            printf("%s:%d:%d: %s\n",
                   jf ? json_object_get_string(jf) : "",
                   jl ? json_object_get_int(jl) : 0,
                   jc ? json_object_get_int(jc) : 0,
                   je ? json_object_get_string(je) : "");
        }
    }

    json_object_put(root);
    free(response_text);
    return IDEBRIDGE_EXIT_OK;
}

static int run_build(bool json_output, const char* profile, const CliGlobalOptions* opts) {
    json_object* args = json_object_new_object();
    if (profile && *profile) json_object_object_add(args, "profile", json_object_new_string(profile));

    char* response_text = NULL;
    json_object* root = NULL;
    int rc = send_request("build", args, opts, &response_text, &root);
    if (rc != IDEBRIDGE_EXIT_OK) return rc;

    json_object* result = NULL;
    rc = print_or_validate_response(json_output, response_text, root, &result);
    if (rc != IDEBRIDGE_EXIT_OK || json_output) return rc;

    json_object *j=NULL;
    int exit_code = 1;
    const char* status = "failed";
    if (result && json_object_object_get_ex(result, "exit_code", &j) && j) exit_code = json_object_get_int(j);
    if (result && json_object_object_get_ex(result, "status", &j) && j) status = json_object_get_string(j);

    printf("build: status=%s exit_code=%d\n", status ? status : "", exit_code);
    if (result && json_object_object_get_ex(result, "output", &j) && j) {
        const char* out = json_object_get_string(j);
        if (out && *out) {
            printf("%s\n", out);
        }
    }

    json_object_put(root);
    free(response_text);
    return (exit_code == 0) ? IDEBRIDGE_EXIT_OK : IDEBRIDGE_EXIT_SERVER;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage(stderr);
        return IDEBRIDGE_EXIT_USAGE;
    }

    const char* cmd = argv[1];
    bool json_output = false;
    int max_items = -1;
    const char* file_filter = NULL;
    bool top_level_only = false;
    const char* open_target = NULL;
    bool graph = false;
    bool regex_mode = false;
    const char* search_pattern = NULL;
    const char* files_csv = NULL;
    const char* profile = NULL;
    const char* edit_apply_file = NULL;
    const char* out_path = NULL;
    bool no_hash_check = false;
    CliGlobalOptions opts = {
        .socket_override = NULL,
        .token_override = NULL,
        .timeout_ms = 4000,
        .spill_file = NULL,
    };

    for (int i = 2; i < argc; ++i) {
        if (strcmp(argv[i], "--json") == 0) {
            json_output = true;
        } else if ((strcmp(argv[i], "--help") == 0) || (strcmp(argv[i], "-h") == 0)) {
            print_usage(stdout);
            return IDEBRIDGE_EXIT_OK;
        } else if (strcmp(argv[i], "--max") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "idebridge: --max requires an integer\n");
                return IDEBRIDGE_EXIT_USAGE;
            }
            max_items = atoi(argv[++i]);
            if (max_items < 0) max_items = -1;
        } else if (strcmp(argv[i], "--file") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "idebridge: --file requires a path\n");
                return IDEBRIDGE_EXIT_USAGE;
            }
            file_filter = argv[++i];
        } else if (strcmp(argv[i], "--top_level_only") == 0) {
            top_level_only = true;
        } else if (strcmp(argv[i], "--graph") == 0) {
            graph = true;
        } else if (strcmp(argv[i], "--regex") == 0) {
            regex_mode = true;
        } else if (strcmp(argv[i], "--files") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "idebridge: --files requires csv paths\n");
                return IDEBRIDGE_EXIT_USAGE;
            }
            files_csv = argv[++i];
        } else if (strcmp(argv[i], "--profile") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "idebridge: --profile requires debug|release\n");
                return IDEBRIDGE_EXIT_USAGE;
            }
            profile = argv[++i];
        } else if (strcmp(argv[i], "--apply") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "idebridge: --apply requires a diff file path\n");
                return IDEBRIDGE_EXIT_USAGE;
            }
            edit_apply_file = argv[++i];
        } else if (strcmp(argv[i], "--no_hash_check") == 0) {
            no_hash_check = true;
        } else if (strcmp(argv[i], "--socket") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "idebridge: --socket requires a path\n");
                return IDEBRIDGE_EXIT_USAGE;
            }
            opts.socket_override = argv[++i];
        } else if (strcmp(argv[i], "--token") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "idebridge: --token requires a value\n");
                return IDEBRIDGE_EXIT_USAGE;
            }
            opts.token_override = argv[++i];
        } else if (strcmp(argv[i], "--timeout_ms") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "idebridge: --timeout_ms requires an integer\n");
                return IDEBRIDGE_EXIT_USAGE;
            }
            opts.timeout_ms = atoi(argv[++i]);
            if (opts.timeout_ms <= 0) {
                fprintf(stderr, "idebridge: --timeout_ms must be > 0\n");
                return IDEBRIDGE_EXIT_USAGE;
            }
        } else if (strcmp(argv[i], "--spill_file") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "idebridge: --spill_file requires a path\n");
                return IDEBRIDGE_EXIT_USAGE;
            }
            opts.spill_file = argv[++i];
        } else if (strcmp(argv[i], "--out") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "idebridge: --out requires a path\n");
                return IDEBRIDGE_EXIT_USAGE;
            }
            out_path = argv[++i];
        } else if (strcmp(cmd, "open") == 0 && !open_target) {
            open_target = argv[i];
        } else if (strcmp(cmd, "search") == 0 && !search_pattern) {
            search_pattern = argv[i];
        } else {
            fprintf(stderr, "idebridge: unknown option or argument: %s\n", argv[i]);
            return IDEBRIDGE_EXIT_USAGE;
        }
    }

    if (strcmp(cmd, "ping") == 0) return run_ping(json_output, &opts);
    if (strcmp(cmd, "diag") == 0) return run_diag(json_output, max_items, &opts);
    if (strcmp(cmd, "diag-pack") == 0) return run_diag_pack(out_path, max_items, &opts);
    if (strcmp(cmd, "diag-dataset") == 0) return run_diag_dataset(out_path, max_items, &opts);
    if (strcmp(cmd, "symbols") == 0) return run_symbols(json_output, file_filter, top_level_only, max_items, &opts);
    if (strcmp(cmd, "open") == 0) {
        if (!open_target) {
            fprintf(stderr, "idebridge: open requires <path:line:col>\n");
            return IDEBRIDGE_EXIT_USAGE;
        }
        return run_open(json_output, open_target, &opts);
    }
    if (strcmp(cmd, "includes") == 0) return run_includes(json_output, graph, &opts);
    if (strcmp(cmd, "search") == 0) return run_search(json_output, search_pattern, regex_mode, max_items, files_csv, &opts);
    if (strcmp(cmd, "build") == 0) return run_build(json_output, profile, &opts);
    if (strcmp(cmd, "edit") == 0) return run_edit_apply(json_output, edit_apply_file, no_hash_check, &opts);

    fprintf(stderr, "idebridge: unknown subcommand: %s\n", cmd);
    print_usage(stderr);
    return IDEBRIDGE_EXIT_USAGE;
}
