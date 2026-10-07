#ifndef IDE_IPC_SESSION_EXPORT_H
#define IDE_IPC_SESSION_EXPORT_H

#include <stdbool.h>
#include <stdint.h>

bool ide_ipc_agent_session_write(const char* project_root,
                                 const char* socket_path,
                                 const char* session_id,
                                 const char* auth_token);
bool ide_ipc_agent_session_write_generation(const char* project_root, const char* socket_path,
                                           const char* session_id, const char* auth_token, uint64_t generation);

void ide_ipc_agent_session_clear(const char* session_id);

#endif
