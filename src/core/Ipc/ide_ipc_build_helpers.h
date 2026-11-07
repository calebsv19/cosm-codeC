#ifndef IDE_IPC_BUILD_HELPERS_H
#define IDE_IPC_BUILD_HELPERS_H

#include <json-c/json.h>
#include <stdbool.h>
void ide_ipc_build_set_stopping(bool stopping);

json_object* ide_ipc_build_build_result(json_object* args,
                                        const char* project_root,
                                        json_object** error_out);

#endif
