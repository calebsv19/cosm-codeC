#include "core/Ipc/ide_ipc_server.h"
#include "core/Diagnostics/diagnostics_engine.h"

#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static int parse_json_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return -1;
    char buf[65536];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    buf[n] = '\0';
    json_object* root = json_tokener_parse(buf);
    if (!root) return -1;
    json_object_put(root);
    return 0;
}

static int run_cmd_expect(const char* cmd, int expected_exit) {
    int rc = system(cmd);
    if (rc == -1 || !WIFEXITED(rc)) return -1;
    int code = WEXITSTATUS(rc);
    return (code == expected_exit) ? 0 : -1;
}

int main(void) {
    const char* xdg_root = "/tmp/idebridge_phase5_xdg_cache";
    const char* workspace = "/tmp/idebridge_phase5_workspace";
    mkdir(xdg_root, 0755);
    mkdir(workspace, 0755);
    setenv("XDG_CACHE_HOME", xdg_root, 1);

    clearDiagnostics();
    addDiagnostic("/tmp/idebridge_phase5_workspace/src/main.c", 12, 4, "example warning", DIAG_SEVERITY_WARNING);
    addDiagnostic("/tmp/idebridge_phase5_workspace/src/main.c", 18, 2, "example error", DIAG_SEVERITY_ERROR);

    if (!ide_ipc_start(workspace)) {
        fprintf(stderr, "failed to start IPC\n");
        return 1;
    }

    const char* socket_path = ide_ipc_socket_path();
    if (!socket_path || !*socket_path) {
        fprintf(stderr, "missing socket path\n");
        ide_ipc_stop();
        return 1;
    }

    if (strstr(socket_path, "/tmp/idebridge_phase5_xdg_cache/caleb_ide/sock/") == NULL) {
        fprintf(stderr, "socket path does not use XDG cache root: %s\n", socket_path);
        ide_ipc_stop();
        return 1;
    }

    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "./idebridge ping --json --socket '%s' >/tmp/idebridge_phase5_ping.json 2>/tmp/idebridge_phase5_ping.err", socket_path);
    if (run_cmd_expect(cmd, 0) != 0) {
        fprintf(stderr, "ping with --socket failed\n");
        ide_ipc_stop();
        return 1;
    }
    if (parse_json_file("/tmp/idebridge_phase5_ping.json") != 0) {
        fprintf(stderr, "invalid ping json output\n");
        ide_ipc_stop();
        return 1;
    }

    snprintf(cmd, sizeof(cmd), "./idebridge ping --json --socket '/tmp/idebridge_missing.sock' >/tmp/idebridge_phase5_bad.json 2>/tmp/idebridge_phase5_bad.err");
    if (run_cmd_expect(cmd, 3) != 0) {
        fprintf(stderr, "invalid socket did not return connect exit code\n");
        ide_ipc_stop();
        return 1;
    }

    snprintf(cmd, sizeof(cmd), "./idebridge diag --json --socket '%s' --spill_file /tmp/idebridge_phase5_spill.json >/tmp/idebridge_phase5_diag.json 2>/tmp/idebridge_phase5_diag.err", socket_path);
    if (run_cmd_expect(cmd, 0) != 0) {
        fprintf(stderr, "diag with spill file failed\n");
        ide_ipc_stop();
        return 1;
    }
    if (parse_json_file("/tmp/idebridge_phase5_spill.json") != 0) {
        fprintf(stderr, "invalid spill json\n");
        ide_ipc_stop();
        return 1;
    }

    ide_ipc_stop();
    printf("idebridge_phase5_check: ok\n");
    return 0;
}
