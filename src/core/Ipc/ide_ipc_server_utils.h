#ifndef IDE_IPC_SERVER_UTILS_H
#define IDE_IPC_SERVER_UTILS_H

#include <json-c/json.h>
#include <stdbool.h>
#include <stddef.h>

bool ide_ipc_generate_auth_token_hex(char out[65]);
bool ide_ipc_build_socket_path(char* out, size_t out_cap, char* out_session, size_t session_cap);
bool ide_ipc_verify_edit_hashes(const char* project_root,
                                const char* diff_text,
                                bool check_hash,
                                json_object* hashes_obj,
                                char* error_out,
                                size_t error_cap);
json_object* ide_ipc_build_error_obj(const char* code, const char* message, const char* details);
char* ide_ipc_build_response_json(const char* id, bool ok, json_object* result, json_object* error);

#endif
