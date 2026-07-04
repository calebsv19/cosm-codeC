#include "startup_diagnostics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static IdeStartupDiagnosticsSnapshot g_startup_diag;

static bool parse_bool_env(const char* value) {
    if (!value || !value[0]) return false;
    return strcmp(value, "1") == 0 ||
           strcasecmp(value, "true") == 0 ||
           strcasecmp(value, "yes") == 0 ||
           strcasecmp(value, "on") == 0;
}

static bool diagnostics_log_enabled(void) {
    return parse_bool_env(getenv("IDE_STARTUP_DIAG_LOG")) ||
           parse_bool_env(getenv("IDE_STARTUP_DIAGNOSTICS"));
}

static void safe_copy(char* out, size_t out_cap, const char* text) {
    if (!out || out_cap == 0) return;
    out[0] = '\0';
    if (!text) return;
    strncpy(out, text, out_cap - 1);
    out[out_cap - 1] = '\0';
}

static const char* text_or_none(const char* text) {
    return (text && text[0]) ? text : "(none)";
}

void ide_startup_diagnostics_reset(void) {
    memset(&g_startup_diag, 0, sizeof(g_startup_diag));
}

void ide_startup_diagnostics_record_runtime_paths(bool ok,
                                                  const char* resource_root,
                                                  const char* source_label,
                                                  const char* executable_dir) {
    g_startup_diag.runtime_paths_recorded = true;
    g_startup_diag.runtime_paths_ok = ok;
    safe_copy(g_startup_diag.runtime_resource_root,
              sizeof(g_startup_diag.runtime_resource_root),
              resource_root);
    safe_copy(g_startup_diag.runtime_resource_source,
              sizeof(g_startup_diag.runtime_resource_source),
              source_label);
    safe_copy(g_startup_diag.runtime_executable_dir,
              sizeof(g_startup_diag.runtime_executable_dir),
              executable_dir);
}

void ide_startup_diagnostics_record_workspace_selection(const char* requested_path,
                                                        bool requested_valid,
                                                        const char* default_path,
                                                        bool default_valid,
                                                        const char* selected_path,
                                                        bool used_fallback,
                                                        bool available) {
    g_startup_diag.workspace_recorded = true;
    safe_copy(g_startup_diag.workspace_requested_path,
              sizeof(g_startup_diag.workspace_requested_path),
              requested_path);
    g_startup_diag.workspace_requested_valid = requested_valid;
    safe_copy(g_startup_diag.workspace_default_path,
              sizeof(g_startup_diag.workspace_default_path),
              default_path);
    g_startup_diag.workspace_default_valid = default_valid;
    safe_copy(g_startup_diag.workspace_selected_path,
              sizeof(g_startup_diag.workspace_selected_path),
              selected_path);
    g_startup_diag.workspace_used_fallback = used_fallback;
    g_startup_diag.workspace_available = available;
}

void ide_startup_diagnostics_record_ipc_start(bool attempted,
                                              bool running,
                                              const char* status,
                                              const char* socket_path,
                                              const char* session_id) {
    g_startup_diag.ipc_recorded = true;
    g_startup_diag.ipc_attempted = attempted;
    g_startup_diag.ipc_running = running;
    safe_copy(g_startup_diag.ipc_status,
              sizeof(g_startup_diag.ipc_status),
              status && status[0] ? status : (running ? "listening" : "unavailable"));
    safe_copy(g_startup_diag.ipc_socket_path,
              sizeof(g_startup_diag.ipc_socket_path),
              socket_path);
    safe_copy(g_startup_diag.ipc_session_id,
              sizeof(g_startup_diag.ipc_session_id),
              session_id);
}

bool ide_startup_diagnostics_snapshot(IdeStartupDiagnosticsSnapshot* out_snapshot) {
    if (!out_snapshot) return false;
    *out_snapshot = g_startup_diag;
    return true;
}

bool ide_startup_diagnostics_format_summary(char* out,
                                            size_t out_cap,
                                            const IdeStartupDiagnosticsSnapshot* snapshot) {
    const IdeStartupDiagnosticsSnapshot* s = snapshot ? snapshot : &g_startup_diag;
    int written;
    if (!out || out_cap == 0) return false;
    written = snprintf(out,
                       out_cap,
                       "[StartupDiagnostics] runtime=recorded:%d ok:%d source:%s root:%s exe:%s "
                       "workspace=recorded:%d requested_valid:%d default_valid:%d fallback:%d "
                       "available:%d selected:%s ipc=recorded:%d attempted:%d running:%d "
                       "status:%s socket:%s session:%s",
                       s->runtime_paths_recorded ? 1 : 0,
                       s->runtime_paths_ok ? 1 : 0,
                       text_or_none(s->runtime_resource_source),
                       text_or_none(s->runtime_resource_root),
                       text_or_none(s->runtime_executable_dir),
                       s->workspace_recorded ? 1 : 0,
                       s->workspace_requested_valid ? 1 : 0,
                       s->workspace_default_valid ? 1 : 0,
                       s->workspace_used_fallback ? 1 : 0,
                       s->workspace_available ? 1 : 0,
                       text_or_none(s->workspace_selected_path),
                       s->ipc_recorded ? 1 : 0,
                       s->ipc_attempted ? 1 : 0,
                       s->ipc_running ? 1 : 0,
                       text_or_none(s->ipc_status),
                       text_or_none(s->ipc_socket_path),
                       text_or_none(s->ipc_session_id));
    if (written < 0) {
        out[0] = '\0';
        return false;
    }
    return (size_t)written < out_cap;
}

void ide_startup_diagnostics_print_if_enabled(FILE* stream) {
    char line[4096];
    FILE* out = stream ? stream : stderr;
    if (!diagnostics_log_enabled()) return;
    if (!ide_startup_diagnostics_format_summary(line, sizeof(line), NULL)) return;
    fprintf(out, "%s\n", line);
}
