#include "core/Ipc/ide_ipc_server.h"

#include <errno.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

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
        if (errno == EINTR) continue;
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

static int expect_ok_response(const char* response, const char* expected_cmd) {
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

    json_object* j_proto = NULL;
    if (!json_object_object_get_ex(j_result, "proto", &j_proto) || json_object_get_int(j_proto) != IDE_IPC_PROTO_VERSION) {
        json_object_put(root);
        return -1;
    }

    json_object* j_session = NULL;
    if (!json_object_object_get_ex(j_result, "session_id", &j_session) || !json_object_get_string(j_session)) {
        json_object_put(root);
        return -1;
    }

    (void)expected_cmd;
    json_object_put(root);
    return 0;
}

static int expect_error_code(const char* response, const char* code) {
    json_object* root = json_tokener_parse(response);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return -1;
    }

    json_object* j_ok = NULL;
    json_object* j_error = NULL;
    json_object* j_code = NULL;

    if (!json_object_object_get_ex(root, "ok", &j_ok) || json_object_get_boolean(j_ok)) {
        json_object_put(root);
        return -1;
    }

    if (!json_object_object_get_ex(root, "error", &j_error) || !j_error) {
        json_object_put(root);
        return -1;
    }

    if (!json_object_object_get_ex(j_error, "code", &j_code) || !j_code) {
        json_object_put(root);
        return -1;
    }

    const char* got = json_object_get_string(j_code);
    int rc = (got && strcmp(got, code) == 0) ? 0 : -1;
    json_object_put(root);
    return rc;
}

int main(void) {
    if (!ide_ipc_start("/tmp/idebridge_phase1_project")) {
        fprintf(stderr, "failed to start IPC server\n");
        return 1;
    }

    const char* socket_path = ide_ipc_socket_path();
    if (!socket_path || !*socket_path) {
        fprintf(stderr, "missing socket path\n");
        ide_ipc_stop();
        return 1;
    }

    struct stat st;
    if (stat(socket_path, &st) != 0) {
        fprintf(stderr, "socket file missing\n");
        ide_ipc_stop();
        return 1;
    }

    char response[16384];

    const char* ping_req = "{\"id\":\"t1\",\"proto\":1,\"cmd\":\"ping\",\"args\":{}}";
    if (send_and_recv(socket_path, ping_req, response, sizeof(response)) != 0) {
        fprintf(stderr, "ping request failed\n");
        ide_ipc_stop();
        return 1;
    }
    if (expect_ok_response(response, "ping") != 0) {
        fprintf(stderr, "ping response validation failed: %s\n", response);
        ide_ipc_stop();
        return 1;
    }

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "MYIDE_SOCKET='%s' ./idebridge ping >/tmp/idebridge_ping_human.txt 2>/tmp/idebridge_ping_human.err", socket_path);
    int human_rc = system(cmd);
    if (human_rc == -1 || !WIFEXITED(human_rc) || WEXITSTATUS(human_rc) != 0) {
        fprintf(stderr, "idebridge human ping failed\n");
        ide_ipc_stop();
        return 1;
    }

    snprintf(cmd, sizeof(cmd), "MYIDE_SOCKET='%s' ./idebridge ping --json >/tmp/idebridge_ping_json.txt 2>/tmp/idebridge_ping_json.err", socket_path);
    int json_rc = system(cmd);
    if (json_rc == -1 || !WIFEXITED(json_rc) || WEXITSTATUS(json_rc) != 0) {
        fprintf(stderr, "idebridge json ping failed\n");
        ide_ipc_stop();
        return 1;
    }

    FILE* json_file = fopen("/tmp/idebridge_ping_json.txt", "rb");
    if (!json_file) {
        fprintf(stderr, "failed to open idebridge json output\n");
        ide_ipc_stop();
        return 1;
    }
    char json_buf[8192];
    size_t json_len = fread(json_buf, 1, sizeof(json_buf) - 1, json_file);
    fclose(json_file);
    json_buf[json_len] = '\0';
    json_object* parsed = (json_len > 0) ? json_tokener_parse(json_buf) : NULL;
    if (!parsed) {
        fprintf(stderr, "idebridge json output invalid\n");
        ide_ipc_stop();
        return 1;
    }
    json_object_put(parsed);

    if (send_and_recv(socket_path, "{", response, sizeof(response)) != 0) {
        fprintf(stderr, "malformed request failed\n");
        ide_ipc_stop();
        return 1;
    }
    if (expect_error_code(response, "bad_json") != 0) {
        fprintf(stderr, "malformed request code mismatch: %s\n", response);
        ide_ipc_stop();
        return 1;
    }

    const char* unknown_req = "{\"id\":\"t2\",\"proto\":1,\"cmd\":\"unknown\",\"args\":{}}";
    if (send_and_recv(socket_path, unknown_req, response, sizeof(response)) != 0) {
        fprintf(stderr, "unknown request failed\n");
        ide_ipc_stop();
        return 1;
    }
    if (expect_error_code(response, "unknown_cmd") != 0) {
        fprintf(stderr, "unknown command code mismatch: %s\n", response);
        ide_ipc_stop();
        return 1;
    }

    char path_copy[256];
    snprintf(path_copy, sizeof(path_copy), "%s", socket_path);
    ide_ipc_stop();

    if (access(path_copy, F_OK) == 0) {
        fprintf(stderr, "socket file still exists after stop\n");
        return 1;
    }

    printf("idebridge_phase1_check: ok\n");
    return 0;
}
