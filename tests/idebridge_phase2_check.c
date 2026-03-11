#include "core/Ipc/ide_ipc_server.h"
#include "core/Diagnostics/diagnostics_engine.h"
#include "core/BuildSystem/build_diagnostics.h"
#include "core/Analysis/analysis_symbols_store.h"

#include <json-c/json.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

static bool s_open_called = false;

static bool open_stub(const char* path,
                      int line,
                      int col,
                      char* error_out,
                      size_t error_out_cap,
                      void* userdata) {
    (void)line;
    (void)col;
    (void)userdata;
    s_open_called = true;
    if (path && strstr(path, "missing") != NULL) {
        snprintf(error_out, error_out_cap, "Path not found");
        return false;
    }
    return true;
}

static int connect_socket(const char* socket_path) {
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socket_path);

    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        close(fd);
        return -1;
    }

    return fd;
}

static int send_and_recv(const char* socket_path, const char* req, char* out, size_t out_cap) {
    int fd = connect_socket(socket_path);
    if (fd < 0) return -1;

    size_t req_len = strlen(req);
    if (write(fd, req, req_len) != (ssize_t)req_len || write(fd, "\n", 1) != 1) {
        close(fd);
        return -1;
    }

    size_t used = 0;
    while (used + 1 < out_cap) {
        ssize_t n = read(fd, out + used, out_cap - used - 1);
        if (n > 0) {
            used += (size_t)n;
            continue;
        }
        if (n == 0) break;
        close(fd);
        return -1;
    }

    close(fd);
    while (used > 0 && (out[used - 1] == '\n' || out[used - 1] == '\r')) {
        used--;
    }
    out[used] = '\0';
    return 0;
}

static int recv_with_pump(int fd, char* out, size_t out_cap) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    size_t used = 0;
    bool got_data = false;
    for (int i = 0; i < 1200 && used + 1 < out_cap; ++i) {
        ide_ipc_pump();
        ssize_t n = read(fd, out + used, out_cap - used - 1);
        if (n > 0) {
            used += (size_t)n;
            got_data = true;
            continue;
        }
        if (n == 0 && got_data) {
            break;
        }
        usleep(5000);
    }

    if (!got_data) return -1;
    while (used > 0 && (out[used - 1] == '\n' || out[used - 1] == '\r')) {
        used--;
    }
    out[used] = '\0';
    return 0;
}

static int expect_ok_with_result(const char* response, json_object** result_out) {
    json_object* root = json_tokener_parse(response);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return -1;
    }

    json_object* j_ok = NULL;
    json_object* j_result = NULL;
    if (!json_object_object_get_ex(root, "ok", &j_ok) || !json_object_get_boolean(j_ok)) {
        json_object_put(root);
        return -1;
    }

    if (!json_object_object_get_ex(root, "result", &j_result) || !j_result) {
        json_object_put(root);
        return -1;
    }

    *result_out = root;
    return 0;
}

static int expect_err_code(const char* response, const char* code) {
    json_object* root = json_tokener_parse(response);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return -1;
    }

    json_object* j_ok = NULL;
    json_object* j_err = NULL;
    json_object* j_code = NULL;
    if (!json_object_object_get_ex(root, "ok", &j_ok) || json_object_get_boolean(j_ok)) {
        json_object_put(root);
        return -1;
    }
    if (!json_object_object_get_ex(root, "error", &j_err) || !j_err) {
        json_object_put(root);
        return -1;
    }
    if (!json_object_object_get_ex(j_err, "code", &j_code) || !j_code) {
        json_object_put(root);
        return -1;
    }

    const char* got = json_object_get_string(j_code);
    int rc = (got && strcmp(got, code) == 0) ? 0 : -1;
    json_object_put(root);
    return rc;
}

int main(void) {
    initDiagnosticsEngine();
    clearDiagnostics();
    build_diagnostics_clear();
    analysis_symbols_store_clear();

    addDiagnostic("/tmp/proj/src/a.c", 12, 4, "unused variable", DIAG_SEVERITY_WARNING);
    addDiagnostic("/tmp/proj/src/a.c", 19, 2, "null dereference", DIAG_SEVERITY_ERROR);

    const char* build_chunk = "/tmp/proj/src/b.c:3:1: warning: build warn\n/tmp/proj/src/b.c:4:2: error: build err\n";
    build_diagnostics_feed_chunk(build_chunk, strlen(build_chunk));

    FisicsSymbol syms[2];
    memset(syms, 0, sizeof(syms));
    syms[0].name = "foo";
    syms[0].file_path = "/tmp/proj/src/a.c";
    syms[0].start_line = 10;
    syms[0].start_col = 1;
    syms[0].end_line = 20;
    syms[0].end_col = 1;
    syms[0].kind = FISICS_SYMBOL_FUNCTION;
    syms[0].return_type = "int";

    syms[1].name = "field_a";
    syms[1].file_path = "/tmp/proj/src/a.c";
    syms[1].start_line = 6;
    syms[1].start_col = 3;
    syms[1].end_line = 6;
    syms[1].end_col = 9;
    syms[1].kind = FISICS_SYMBOL_FIELD;
    syms[1].parent_name = "MyStruct";
    syms[1].parent_kind = FISICS_SYMBOL_STRUCT;

    analysis_symbols_store_upsert("/tmp/proj/src/a.c", syms, 2);

    if (!ide_ipc_start("/tmp/proj")) {
        fprintf(stderr, "failed to start IPC server\n");
        return 1;
    }
    ide_ipc_set_open_handler(open_stub, NULL);

    const char* socket_path = ide_ipc_socket_path();
    if (!socket_path || !*socket_path) {
        fprintf(stderr, "missing socket path\n");
        ide_ipc_stop();
        return 1;
    }
    const char* auth_token = ide_ipc_auth_token();
    if (!auth_token || !*auth_token) {
        fprintf(stderr, "missing auth token\n");
        ide_ipc_stop();
        return 1;
    }

    char response[32768];

    const char* diag_req = "{\"id\":\"d1\",\"proto\":1,\"cmd\":\"diag\",\"args\":{\"max\":2}}";
    if (send_and_recv(socket_path, diag_req, response, sizeof(response)) != 0) {
        fprintf(stderr, "diag request failed\n");
        ide_ipc_stop();
        return 1;
    }
    json_object* root = NULL;
    if (expect_ok_with_result(response, &root) != 0) {
        fprintf(stderr, "diag response invalid: %s\n", response);
        ide_ipc_stop();
        return 1;
    }
    json_object* result = NULL;
    json_object* j = NULL;
    json_object_object_get_ex(root, "result", &result);
    json_object_object_get_ex(result, "total_count", &j);
    if (!j || json_object_get_int(j) != 4) {
        fprintf(stderr, "diag total_count mismatch\n");
        json_object_put(root);
        ide_ipc_stop();
        return 1;
    }
    json_object_object_get_ex(result, "returned_count", &j);
    if (!j || json_object_get_int(j) != 2) {
        fprintf(stderr, "diag returned_count mismatch\n");
        json_object_put(root);
        ide_ipc_stop();
        return 1;
    }
    json_object_put(root);

    const char* symbols_req = "{\"id\":\"s1\",\"proto\":1,\"cmd\":\"symbols\",\"args\":{\"file\":\"src/a.c\",\"top_level_only\":true}}";
    if (send_and_recv(socket_path, symbols_req, response, sizeof(response)) != 0) {
        fprintf(stderr, "symbols request failed\n");
        ide_ipc_stop();
        return 1;
    }
    root = NULL;
    if (expect_ok_with_result(response, &root) != 0) {
        fprintf(stderr, "symbols response invalid: %s\n", response);
        ide_ipc_stop();
        return 1;
    }
    json_object_object_get_ex(root, "result", &result);
    json_object_object_get_ex(result, "total_count", &j);
    if (!j || json_object_get_int(j) != 1) {
        fprintf(stderr, "symbols top_level_only mismatch\n");
        json_object_put(root);
        ide_ipc_stop();
        return 1;
    }
    json_object_put(root);

    // open success: requires pump on main thread
    s_open_called = false;
    char open_req[2048];
    snprintf(open_req, sizeof(open_req),
             "{\"id\":\"o1\",\"proto\":1,\"cmd\":\"open\",\"auth_token\":\"%s\",\"args\":{\"path\":\"src/a.c\",\"line\":7,\"col\":2}}",
             auth_token);
    int fd = connect_socket(socket_path);
    if (fd < 0) {
        fprintf(stderr, "open connect failed\n");
        ide_ipc_stop();
        return 1;
    }
    write(fd, open_req, strlen(open_req));
    write(fd, "\n", 1);
    if (recv_with_pump(fd, response, sizeof(response)) != 0) {
        fprintf(stderr, "open success read failed\n");
        close(fd);
        ide_ipc_stop();
        return 1;
    }
    close(fd);
    if (!s_open_called || expect_ok_with_result(response, &root) != 0) {
        fprintf(stderr, "open success invalid: %s\n", response);
        ide_ipc_stop();
        return 1;
    }
    json_object_put(root);

    // open failure
    s_open_called = false;
    char open_fail_req[2048];
    snprintf(open_fail_req, sizeof(open_fail_req),
             "{\"id\":\"o2\",\"proto\":1,\"cmd\":\"open\",\"auth_token\":\"%s\",\"args\":{\"path\":\"missing.c\",\"line\":1,\"col\":1}}",
             auth_token);
    fd = connect_socket(socket_path);
    if (fd < 0) {
        fprintf(stderr, "open fail connect failed\n");
        ide_ipc_stop();
        return 1;
    }
    write(fd, open_fail_req, strlen(open_fail_req));
    write(fd, "\n", 1);
    if (recv_with_pump(fd, response, sizeof(response)) != 0) {
        fprintf(stderr, "open failure read failed\n");
        close(fd);
        ide_ipc_stop();
        return 1;
    }
    close(fd);
    if (!s_open_called || expect_err_code(response, "open_failed") != 0) {
        fprintf(stderr, "open failure invalid: %s\n", response);
        ide_ipc_stop();
        return 1;
    }

    ide_ipc_stop();
    analysis_symbols_store_clear();

    printf("idebridge_phase2_check: ok\n");
    return 0;
}
