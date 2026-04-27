#ifndef IDE_IPC_BUILD_HELPERS_H
#define IDE_IPC_BUILD_HELPERS_H

#include <json-c/json.h>

json_object* ide_ipc_build_build_result(json_object* args,
                                        const char* project_root,
                                        json_object** error_out);

#endif
