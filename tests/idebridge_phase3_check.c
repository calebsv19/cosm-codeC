#include "core/Ipc/ide_ipc_server.h"
#include "core/Analysis/analysis_build_graph_store.h"
#include "core/Analysis/analysis_memory_report_store.h"
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

    char graph_path[1024];
    snprintf(graph_path, sizeof(graph_path), "%s/build_graph.json", workspace);
    const char* graph_json =
        "{"
        "\"schema\":\"fisiCs.build_graph\","
        "\"version\":0,"
        "\"project_root\":\"/tmp/idebridge_phase3_workspace\","
        "\"mode\":\"dry_run\","
        "\"partial\":false,"
        "\"fatal\":false,"
        "\"diagnostic_summary\":{\"available\":false,\"total\":0,\"errors\":0,\"warnings\":0,\"notes\":0,\"partial\":false,\"fatal\":false},"
        "\"translation_units\":[{"
        "\"id\":\"tu0\","
        "\"source\":\"/tmp/idebridge_phase3_workspace/src/a.c\","
        "\"object\":\"/tmp/idebridge_phase3_workspace/build/a.o\","
        "\"status\":\"ok\","
        "\"diagnostic_summary\":{\"available\":false,\"total\":0,\"errors\":0,\"warnings\":0,\"notes\":0,\"partial\":false,\"fatal\":false}"
        "}],"
        "\"plan\":{\"schema\":\"fisiCs.build_plan\",\"version\":0,\"dry_run\":true,\"actions\":[{"
        "\"id\":\"compile0\","
        "\"kind\":\"compile\","
        "\"status\":\"planned\","
        "\"will_execute\":false,"
        "\"source\":\"/tmp/idebridge_phase3_workspace/src/a.c\","
        "\"object\":\"/tmp/idebridge_phase3_workspace/build/a.o\","
        "\"diagnostic_summary\":{\"available\":false,\"total\":0,\"errors\":0,\"warnings\":0,\"notes\":0,\"partial\":false,\"fatal\":false}"
        "}]}"
        "}";
    if (write_file(graph_path, graph_json) != 0) {
        fprintf(stderr, "failed to write build graph fixture\n");
        return 1;
    }

    char memory_report_path[1024];
    snprintf(memory_report_path, sizeof(memory_report_path), "%s/memory_report.json", workspace);
    const char* memory_report_json =
        "{"
        "\"profile\":\"memory_check_report_v1\","
        "\"schema_version\":1,"
        "\"runtime\":\"fisics_memory_check\","
        "\"trigger\":\"manual\","
        "\"summary\":{"
        "\"active\":1,"
        "\"leaked_bytes\":21,"
        "\"allocs\":1,"
        "\"frees\":0,"
        "\"double_free\":0,"
        "\"unknown_free\":0,"
        "\"tracker_failures\":0"
        "},"
        "\"leaks\":[{"
        "\"size\":21,"
        "\"allocated_at\":{\"file\":\"memory_check_json_report.c\",\"line\":7}"
        "}]"
        "}";
    if (write_file(memory_report_path, memory_report_json) != 0) {
        fprintf(stderr, "failed to write memory report fixture\n");
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

    const char* graph_req = "{\"id\":\"g1\",\"proto\":1,\"cmd\":\"build_graph\",\"args\":{\"path\":\"build_graph.json\"}}";
    if (send_and_recv(socket_path, graph_req, response, sizeof(response)) != 0) {
        fprintf(stderr, "build_graph request failed\n");
        ide_ipc_stop();
        return 1;
    }
    if (get_ok_result(response, &root, &result) != 0) {
        fprintf(stderr, "build_graph invalid: %s\n", response);
        ide_ipc_stop();
        return 1;
    }
    json_object* ingested = NULL;
    json_object* snapshots = NULL;
    if (!json_object_object_get_ex(result, "ingested", &ingested) ||
        !json_object_get_boolean(ingested) ||
        !json_object_object_get_ex(result, "snapshots", &snapshots) ||
        json_object_array_length(snapshots) != 1) {
        fprintf(stderr, "build_graph ingestion mismatch\n");
        json_object_put(root);
        ide_ipc_stop();
        return 1;
    }
    json_object* snapshot = json_object_array_get_idx(snapshots, 0);
    json_object* mode = NULL;
    json_object* tus = NULL;
    json_object* actions = NULL;
    if (!snapshot ||
        !json_object_object_get_ex(snapshot, "mode", &mode) ||
        strcmp(json_object_get_string(mode), "dry_run") != 0 ||
        !json_object_object_get_ex(snapshot, "translation_units", &tus) ||
        json_object_array_length(tus) != 1 ||
        !json_object_object_get_ex(snapshot, "actions", &actions) ||
        json_object_array_length(actions) != 1) {
        fprintf(stderr, "build_graph snapshot mismatch\n");
        json_object_put(root);
        ide_ipc_stop();
        return 1;
    }
    json_object* action0 = json_object_array_get_idx(actions, 0);
    json_object* action_summary = NULL;
    json_object* available = NULL;
    if (!action0 ||
        !json_object_object_get_ex(action0, "diagnostic_summary", &action_summary) ||
        !json_object_object_get_ex(action_summary, "available", &available) ||
        json_object_get_boolean(available)) {
        fprintf(stderr, "build_graph action diagnostic summary mismatch\n");
        json_object_put(root);
        ide_ipc_stop();
        return 1;
    }
    json_object_put(root);

    const char* memory_req = "{\"id\":\"m1\",\"proto\":1,\"cmd\":\"memory_reports\",\"args\":{\"path\":\"memory_report.json\"}}";
    if (send_and_recv(socket_path, memory_req, response, sizeof(response)) != 0) {
        fprintf(stderr, "memory_reports request failed\n");
        ide_ipc_stop();
        return 1;
    }
    if (get_ok_result(response, &root, &result) != 0) {
        fprintf(stderr, "memory_reports invalid: %s\n", response);
        ide_ipc_stop();
        return 1;
    }
    json_object* reports = NULL;
    json_object* report_count = NULL;
    if (!json_object_object_get_ex(result, "ingested", &ingested) ||
        !json_object_get_boolean(ingested) ||
        !json_object_object_get_ex(result, "report_count", &report_count) ||
        json_object_get_int(report_count) != 1 ||
        !json_object_object_get_ex(result, "reports", &reports) ||
        json_object_array_length(reports) != 1) {
        fprintf(stderr, "memory_reports ingestion mismatch\n");
        json_object_put(root);
        ide_ipc_stop();
        return 1;
    }
    json_object* report = json_object_array_get_idx(reports, 0);
    json_object* report_summary = NULL;
    json_object* leaked_bytes = NULL;
    json_object* leaks = NULL;
    if (!report ||
        !json_object_object_get_ex(report, "summary", &report_summary) ||
        !json_object_object_get_ex(report_summary, "leaked_bytes", &leaked_bytes) ||
        json_object_get_int64(leaked_bytes) != 21 ||
        !json_object_object_get_ex(report, "leaks", &leaks) ||
        json_object_array_length(leaks) != 1) {
        fprintf(stderr, "memory_reports summary mismatch\n");
        json_object_put(root);
        ide_ipc_stop();
        return 1;
    }
    json_object* leak0 = json_object_array_get_idx(leaks, 0);
    json_object* allocated_at = NULL;
    json_object* allocated_file = NULL;
    json_object* allocated_line = NULL;
    if (!leak0 ||
        !json_object_object_get_ex(leak0, "allocated_at", &allocated_at) ||
        !json_object_object_get_ex(allocated_at, "file", &allocated_file) ||
        strcmp(json_object_get_string(allocated_file), "memory_check_json_report.c") != 0 ||
        !json_object_object_get_ex(allocated_at, "line", &allocated_line) ||
        json_object_get_int(allocated_line) != 7) {
        fprintf(stderr, "memory_reports leak mismatch\n");
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
    analysis_build_graph_store_clear();
    analysis_memory_report_store_clear();
    library_index_reset();

    printf("idebridge_phase3_check: ok\n");
    return 0;
}
