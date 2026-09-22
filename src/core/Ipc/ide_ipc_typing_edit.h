#ifndef IDE_IPC_TYPING_EDIT_H
#define IDE_IPC_TYPING_EDIT_H

#include "core/Ipc/ide_ipc_typing.h"

IdeIpcTypingStepResult ide_ipc_typing_edit_step(
    const IdeIpcTypingRequest* request,
    bool begin,
    char* error_out,
    size_t error_out_cap,
    void* userdata,
    struct json_object** result_out);

#endif
