#ifndef IDE_IPC_SESSION_EXPORT_H
#define IDE_IPC_SESSION_EXPORT_H

#include <stdbool.h>

bool ide_ipc_agent_session_write(const char* project_root,
                                 const char* socket_path,
                                 const char* session_id,
                                 const char* auth_token);
void ide_ipc_agent_session_clear(const char* session_id);

#endif
