#ifndef IDE_IPC_QUERY_HELPERS_H
#define IDE_IPC_QUERY_HELPERS_H

#include <json-c/json.h>

json_object* ide_ipc_build_diag_result(json_object* args, const char* project_root);
json_object* ide_ipc_build_symbol_result(json_object* args, const char* project_root);
json_object* ide_ipc_build_token_result(json_object* args, const char* project_root);
json_object* ide_ipc_build_includes_result(json_object* args);

#endif
