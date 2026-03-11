#include "core/Ipc/ide_ipc_server.h"
#include "core/Analysis/library_index.h"

#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
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
        close(fd);
        return -1;
    }
    close(fd);

    while (used > 0 && (out[used - 1] == '\n' || out[used - 1] == '\r')) used--;
    out[used] = '\0';
    return 0;
}

static int get_ok_result(const char* response, json_object** root_out, json_object** result_out) {
    json_object* root = json_tokener_parse(response);
    if (!root || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        return -1;
    }
    json_object* ok = NULL;
    json_object* result = NULL;
    if (!json_object_object_get_ex(root, "ok", &ok) || !json_object_get_boolean(ok)) {
        json_object_put(root);
        return -1;
    }
    if (!json_object_object_get_ex(root, "result", &result) || !result) {
        json_object_put(root);
        return -1;
    }
    *root_out = root;
    *result_out = result;
    return 0;
}

static int write_file(const char* path, const char* body) {
    FILE* f = fopen(path, "w");
    if (!f) return -1;
    fputs(body, f);
    fclose(f);
    return 0;
}

int main(void) {
    const char* workspace = "/tmp/idebridge_phase3_workspace";
    mkdir(workspace, 0755);

    char src_dir[1024];
    snprintf(src_dir, sizeof(src_dir), "%s/src", workspace);
    mkdir(src_dir, 0755);

    char file_a[1024];
    snprintf(file_a, sizeof(file_a), "%s/a.c", src_dir);
    if (write_file(file_a, "#include <stdio.h>\nint alpha = 1;\nint beta = alpha + 2;\n") != 0) {
        fprintf(stderr, "failed to write search fixture\n");
        return 1;
    }

    char makefile_path[1024];
    snprintf(makefile_path, sizeof(makefile_path), "%s/Makefile", workspace);
    if (write_file(makefile_path, "all:\n\t@echo build_ok\n") != 0) {
        fprintf(stderr, "failed to write Makefile\n");
        return 1;
    }

    library_index_begin(workspace);
    library_index_add_include(file_a, "stdio.h", "/usr/include/stdio.h", LIB_INCLUDE_KIND_SYSTEM, LIB_BUCKET_SYSTEM, 1, 1);
    library_index_add_include(file_a, "missing_local.h", NULL, LIB_INCLUDE_KIND_LOCAL, LIB_BUCKET_UNRESOLVED, 2, 1);
    library_index_finalize();

    if (!ide_ipc_start(workspace)) {
        fprintf(stderr, "failed to start IPC\n");
        return 1;
    }

    const char* socket_path = ide_ipc_socket_path();
    if (!socket_path || !*socket_path) {
        fprintf(stderr, "socket missing\n");
        ide_ipc_stop();
        return 1;
    }
    const char* auth_token = ide_ipc_auth_token();
    if (!auth_token || !*auth_token) {
        fprintf(stderr, "auth token missing\n");
        ide_ipc_stop();
        return 1;
    }

    char response[65536];

    const char* includes_req = "{\"id\":\"i1\",\"proto\":1,\"cmd\":\"includes\",\"args\":{\"graph\":true}}";
    if (send_and_recv(socket_path, includes_req, response, sizeof(response)) != 0) {
        fprintf(stderr, "includes request failed\n");
        ide_ipc_stop();
        return 1;
    }
    json_object* root = NULL;
    json_object* result = NULL;
    if (get_ok_result(response, &root, &result) != 0) {
        fprintf(stderr, "includes invalid: %s\n", response);
        ide_ipc_stop();
        return 1;
    }
    json_object* summary = NULL;
    json_object* edges = NULL;
    json_object_object_get_ex(result, "summary", &summary);
    json_object_object_get_ex(result, "edges", &edges);
    json_object* htotal = NULL;
    if (!summary || !json_object_object_get_ex(summary, "headers_total", &htotal) || json_object_get_int(htotal) < 2) {
        fprintf(stderr, "includes summary mismatch\n");
        json_object_put(root);
        ide_ipc_stop();
        return 1;
    }
    if (!edges || json_object_array_length(edges) < 2) {
        fprintf(stderr, "includes graph mismatch\n");
        json_object_put(root);
        ide_ipc_stop();
        return 1;
    }
    json_object_put(root);

    const char* search_req = "{\"id\":\"s1\",\"proto\":1,\"cmd\":\"search\",\"args\":{\"pattern\":\"alpha\",\"regex\":false,\"max\":20}}";
    if (send_and_recv(socket_path, search_req, response, sizeof(response)) != 0) {
        fprintf(stderr, "search request failed\n");
        ide_ipc_stop();
        return 1;
    }
    if (get_ok_result(response, &root, &result) != 0) {
        fprintf(stderr, "search invalid: %s\n", response);
        ide_ipc_stop();
        return 1;
    }
    json_object* mcount = NULL;
    if (!json_object_object_get_ex(result, "match_count", &mcount) || json_object_get_int(mcount) < 2) {
        fprintf(stderr, "search match_count mismatch\n");
        json_object_put(root);
        ide_ipc_stop();
        return 1;
    }
    json_object_put(root);

    char build_req_ok[2048];
    snprintf(build_req_ok, sizeof(build_req_ok),
             "{\"id\":\"b1\",\"proto\":1,\"cmd\":\"build\",\"auth_token\":\"%s\",\"args\":{\"profile\":\"debug\"}}",
             auth_token);
    if (send_and_recv(socket_path, build_req_ok, response, sizeof(response)) != 0) {
        fprintf(stderr, "build request failed\n");
        ide_ipc_stop();
        return 1;
    }
    if (get_ok_result(response, &root, &result) != 0) {
        fprintf(stderr, "build ok invalid: %s\n", response);
        ide_ipc_stop();
        return 1;
    }
    json_object* exit_code = NULL;
    if (!json_object_object_get_ex(result, "exit_code", &exit_code) || json_object_get_int(exit_code) != 0) {
        fprintf(stderr, "build success exit_code mismatch\n");
        json_object_put(root);
        ide_ipc_stop();
        return 1;
    }
    json_object_put(root);

    if (write_file(makefile_path, "all:\n\t@echo fail && false\n") != 0) {
        fprintf(stderr, "failed to rewrite failing Makefile\n");
        ide_ipc_stop();
        return 1;
    }

    char build_req_fail[2048];
    snprintf(build_req_fail, sizeof(build_req_fail),
             "{\"id\":\"b2\",\"proto\":1,\"cmd\":\"build\",\"auth_token\":\"%s\",\"args\":{}}",
             auth_token);
    if (send_and_recv(socket_path, build_req_fail, response, sizeof(response)) != 0) {
        fprintf(stderr, "build fail request failed\n");
        ide_ipc_stop();
        return 1;
    }
    if (get_ok_result(response, &root, &result) != 0) {
        fprintf(stderr, "build fail invalid: %s\n", response);
        ide_ipc_stop();
        return 1;
    }
    if (!json_object_object_get_ex(result, "exit_code", &exit_code) || json_object_get_int(exit_code) == 0) {
        fprintf(stderr, "build failure exit_code mismatch\n");
        json_object_put(root);
        ide_ipc_stop();
        return 1;
    }
    json_object_put(root);

    ide_ipc_stop();
    library_index_reset();

    printf("idebridge_phase3_check: ok\n");
    return 0;
}
