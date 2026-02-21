#include "core/Ipc/ide_ipc_server.h"

#include <json-c/json.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

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

static bool edit_stub(const char* diff_text,
                      char* error_out,
                      size_t error_out_cap,
                      void* userdata,
                      json_object** result_out) {
    (void)userdata;
    if (!diff_text || !strstr(diff_text, "@@")) {
        snprintf(error_out, error_out_cap, "Malformed diff");
        return false;
    }
    json_object* result = json_object_new_object();
    json_object* touched = json_object_new_array();
    json_object_array_add(touched, json_object_new_string("src/a.c"));
    json_object_object_add(result, "touched_files", touched);
    json_object* summary = json_object_new_object();
    json_object_object_add(summary, "total", json_object_new_int(0));
    json_object_object_add(summary, "error", json_object_new_int(0));
    json_object_object_add(summary, "warn", json_object_new_int(0));
    json_object_object_add(summary, "info", json_object_new_int(0));
    json_object_object_add(result, "diagnostics_summary", summary);
    *result_out = result;
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

static int send_then_pump_recv(const char* socket_path, const char* req, char* out, size_t out_cap) {
    int fd = connect_socket(socket_path);
    if (fd < 0) return -1;
    write(fd, req, strlen(req));
    write(fd, "\n", 1);

    for (int i = 0; i < 200; ++i) {
        ide_ipc_pump();
        usleep(5000);
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
    while (used > 0 && (out[used - 1] == '\n' || out[used - 1] == '\r')) used--;
    out[used] = '\0';
    return 0;
}

static int expect_ok(const char* response) {
    json_object* root = json_tokener_parse(response);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return -1;
    }
    json_object* ok = NULL;
    int rc = (json_object_object_get_ex(root, "ok", &ok) && json_object_get_boolean(ok)) ? 0 : -1;
    json_object_put(root);
    return rc;
}

static int expect_apply_result_shape(const char* response) {
    json_object* root = json_tokener_parse(response);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return -1;
    }

    int rc = -1;
    json_object *ok=NULL,*result=NULL,*applied=NULL,*touched=NULL,*summary=NULL;
    if (json_object_object_get_ex(root, "ok", &ok) &&
        json_object_get_boolean(ok) &&
        json_object_object_get_ex(root, "result", &result) && result &&
        json_object_object_get_ex(result, "applied", &applied) && applied &&
        json_object_get_boolean(applied) &&
        json_object_object_get_ex(result, "touched_files", &touched) &&
        touched && json_object_is_type(touched, json_type_array) &&
        json_object_array_length(touched) >= 1 &&
        json_object_object_get_ex(result, "diagnostics_summary", &summary) &&
        summary && json_object_is_type(summary, json_type_object)) {
        rc = 0;
    }

    json_object_put(root);
    return rc;
}

static int expect_error_code(const char* response, const char* code) {
    json_object* root = json_tokener_parse(response);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return -1;
    }
    json_object *ok=NULL,*err=NULL,*c=NULL;
    int rc = -1;
    if (json_object_object_get_ex(root, "ok", &ok) && !json_object_get_boolean(ok) &&
        json_object_object_get_ex(root, "error", &err) && err &&
        json_object_object_get_ex(err, "code", &c) && c) {
        const char* got = json_object_get_string(c);
        if (got && strcmp(got, code) == 0) rc = 0;
    }
    json_object_put(root);
    return rc;
}

int main(void) {
    const char* workspace = "/tmp/idebridge_phase4_workspace";
    mkdir(workspace, 0755);
    char src_dir[1024];
    snprintf(src_dir, sizeof(src_dir), "%s/src", workspace);
    mkdir(src_dir, 0755);

    char file_a[1024];
    snprintf(file_a, sizeof(file_a), "%s/a.c", src_dir);
    FILE* f = fopen(file_a, "w");
    if (!f) return 1;
    fputs("int x = 1;\n", f);
    fclose(f);

    bool ok_hash = false;
    unsigned long long hash_value = fnv1a64_file(file_a, &ok_hash);
    if (!ok_hash) return 1;
    char hash[32];
    snprintf(hash, sizeof(hash), "%016llx", hash_value);

    if (!ide_ipc_start(workspace)) return 1;
    ide_ipc_set_edit_handler(edit_stub, NULL);

    const char* socket_path = ide_ipc_socket_path();
    if (!socket_path || !*socket_path) {
        ide_ipc_stop();
        return 1;
    }

    char req_ok[8192];
    snprintf(req_ok, sizeof(req_ok),
             "{\"id\":\"e1\",\"proto\":1,\"cmd\":\"edit\",\"args\":{\"op\":\"apply\",\"diff\":\"%s\",\"check_hash\":true,\"hashes\":{\"src/a.c\":\"%s\"}}}",
             "--- a/src/a.c\\n+++ b/src/a.c\\n@@ -1,1 +1,1 @@\\n-int x = 1;\\n+int x = 2;\\n",
             hash);

    char response[32768];
    if (send_then_pump_recv(socket_path, req_ok, response, sizeof(response)) != 0) {
        ide_ipc_stop();
        return 1;
    }
    if (expect_apply_result_shape(response) != 0) {
        fprintf(stderr, "edit apply result shape invalid: %s\n", response);
        ide_ipc_stop();
        return 1;
    }
    if (expect_ok(response) != 0) {
        fprintf(stderr, "edit success failed: %s\n", response);
        ide_ipc_stop();
        return 1;
    }

    char req_bad_hash[8192];
    snprintf(req_bad_hash, sizeof(req_bad_hash),
             "{\"id\":\"e2\",\"proto\":1,\"cmd\":\"edit\",\"args\":{\"op\":\"apply\",\"diff\":\"%s\",\"check_hash\":true,\"hashes\":{\"src/a.c\":\"deadbeefdeadbeef\"}}}",
             "--- a/src/a.c\\n+++ b/src/a.c\\n@@ -1,1 +1,1 @@\\n-int x = 1;\\n+int x = 2;\\n");

    if (send_then_pump_recv(socket_path, req_bad_hash, response, sizeof(response)) != 0) {
        ide_ipc_stop();
        return 1;
    }
    if (expect_error_code(response, "hash_mismatch") != 0) {
        fprintf(stderr, "edit hash mismatch not caught: %s\n", response);
        ide_ipc_stop();
        return 1;
    }

    const char* req_no_hash =
        "{\"id\":\"e3\",\"proto\":1,\"cmd\":\"edit\",\"args\":{\"op\":\"apply\",\"diff\":\"--- a/src/a.c\\n+++ b/src/a.c\\n@@ -1,1 +1,1 @@\\n-int x = 1;\\n+int x = 3;\\n\",\"check_hash\":false,\"hashes\":{}}}";
    if (send_then_pump_recv(socket_path, req_no_hash, response, sizeof(response)) != 0) {
        ide_ipc_stop();
        return 1;
    }
    if (expect_ok(response) != 0) {
        fprintf(stderr, "edit no_hash_check failed: %s\n", response);
        ide_ipc_stop();
        return 1;
    }

    const char* req_bad_diff =
        "{\"id\":\"e4\",\"proto\":1,\"cmd\":\"edit\",\"args\":{\"op\":\"apply\",\"diff\":\"not a unified diff\",\"check_hash\":false,\"hashes\":{}}}";
    if (send_then_pump_recv(socket_path, req_bad_diff, response, sizeof(response)) != 0) {
        ide_ipc_stop();
        return 1;
    }
    if (expect_error_code(response, "edit_failed") != 0) {
        fprintf(stderr, "edit malformed diff not rejected: %s\n", response);
        ide_ipc_stop();
        return 1;
    }

    ide_ipc_stop();
    printf("idebridge_phase4_check: ok\n");
    return 0;
}
