#ifndef IDE_IPC_PATH_GUARD_H
#define IDE_IPC_PATH_GUARD_H

#include <stdbool.h>
#include <stddef.h>

// Resolve a request path into a canonical absolute path confined to project_root.
// - Absolute request paths are rejected.
// - The target must already exist on disk.
// - out_abs receives the canonical absolute target path on success.
bool ide_ipc_resolve_workspace_existing_path(const char* project_root,
                                             const char* request_path,
                                             char* out_abs,
                                             size_t out_abs_cap,
                                             char* error_out,
                                             size_t error_out_cap);

#endif
