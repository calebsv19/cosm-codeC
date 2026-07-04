#ifndef IDE_STARTUP_DIAGNOSTICS_H
#define IDE_STARTUP_DIAGNOSTICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#define IDE_STARTUP_DIAG_PATH_CAP 1024
#define IDE_STARTUP_DIAG_LABEL_CAP 64
#define IDE_STARTUP_DIAG_SOCKET_CAP 128

typedef struct IdeStartupDiagnosticsSnapshot {
    bool runtime_paths_recorded;
    bool runtime_paths_ok;
    char runtime_resource_root[IDE_STARTUP_DIAG_PATH_CAP];
    char runtime_resource_source[IDE_STARTUP_DIAG_LABEL_CAP];
    char runtime_executable_dir[IDE_STARTUP_DIAG_PATH_CAP];

    bool workspace_recorded;
    char workspace_requested_path[IDE_STARTUP_DIAG_PATH_CAP];
    bool workspace_requested_valid;
    char workspace_default_path[IDE_STARTUP_DIAG_PATH_CAP];
    bool workspace_default_valid;
    char workspace_selected_path[IDE_STARTUP_DIAG_PATH_CAP];
    bool workspace_used_fallback;
    bool workspace_available;

    bool ipc_recorded;
    bool ipc_attempted;
    bool ipc_running;
    char ipc_status[IDE_STARTUP_DIAG_LABEL_CAP];
    char ipc_socket_path[IDE_STARTUP_DIAG_SOCKET_CAP];
    char ipc_session_id[IDE_STARTUP_DIAG_SOCKET_CAP];
} IdeStartupDiagnosticsSnapshot;

void ide_startup_diagnostics_reset(void);

void ide_startup_diagnostics_record_runtime_paths(bool ok,
                                                  const char* resource_root,
                                                  const char* source_label,
                                                  const char* executable_dir);

void ide_startup_diagnostics_record_workspace_selection(const char* requested_path,
                                                        bool requested_valid,
                                                        const char* default_path,
                                                        bool default_valid,
                                                        const char* selected_path,
                                                        bool used_fallback,
                                                        bool available);

void ide_startup_diagnostics_record_ipc_start(bool attempted,
                                              bool running,
                                              const char* status,
                                              const char* socket_path,
                                              const char* session_id);

bool ide_startup_diagnostics_snapshot(IdeStartupDiagnosticsSnapshot* out_snapshot);

bool ide_startup_diagnostics_format_summary(char* out,
                                            size_t out_cap,
                                            const IdeStartupDiagnosticsSnapshot* snapshot);

void ide_startup_diagnostics_print_if_enabled(FILE* stream);

#endif
