#ifndef IDE_IPC_SERVER_H
#define IDE_IPC_SERVER_H

#include <stdbool.h>
#include <stddef.h>

#define IDE_IPC_PROTO_VERSION 1

struct json_object;

typedef bool (*IdeIpcOpenHandler)(const char* path,
                                  int line,
                                  int col,
                                  char* error_out,
                                  size_t error_out_cap,
                                  void* userdata);

typedef bool (*IdeIpcEditApplyHandler)(const char* diff_text,
                                       char* error_out,
                                       size_t error_out_cap,
                                       void* userdata,
                                       struct json_object** result_out);

bool ide_ipc_start(const char* project_root);
void ide_ipc_stop(void);
bool ide_ipc_is_running(void);
void ide_ipc_pump(void);

void ide_ipc_set_open_handler(IdeIpcOpenHandler handler, void* userdata);
void ide_ipc_set_edit_handler(IdeIpcEditApplyHandler handler, void* userdata);

const char* ide_ipc_socket_path(void);
const char* ide_ipc_session_id(void);
const char* ide_ipc_auth_token(void);

#endif
