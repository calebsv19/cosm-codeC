#ifndef IDE_IPC_WORKSPACE_H
#define IDE_IPC_WORKSPACE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <json-c/json.h>
typedef bool (*IdeWorkspaceHandler)(const char*, char*, size_t);
void ide_ipc_workspace_set_handler(IdeWorkspaceHandler handler);
bool ide_ipc_workspace_request(const char* path, const char* expected_root,
                               uint64_t expected_generation, char* error, size_t cap);
void ide_ipc_workspace_pump(void);
uint64_t ide_ipc_workspace_generation(void);
void ide_ipc_workspace_changed(void);
void ide_ipc_workspace_mark_ready(void);
bool ide_ipc_workspace_ready(void);
#endif
