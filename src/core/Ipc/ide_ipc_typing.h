#ifndef IDE_IPC_TYPING_H
#define IDE_IPC_TYPING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct json_object;

#define IDE_IPC_TYPING_MAX_TEXT_BYTES 4096

typedef struct {
    const char* path;
    const char* expected_before_fnv64;
    const char* expected_after_fnv64;
    const char* expected_text;
    const char* replacement_text;
    int start_line;
    int start_column;
    int end_line;
    int end_column;
    int cadence_ms;
} IdeIpcTypingRequest;

typedef enum {
    IDE_IPC_TYPING_STEP_CONTINUE = 0,
    IDE_IPC_TYPING_STEP_COMPLETE,
    IDE_IPC_TYPING_STEP_FAILED,
} IdeIpcTypingStepResult;

typedef IdeIpcTypingStepResult (*IdeIpcTypingHandler)(
    const IdeIpcTypingRequest* request,
    bool begin,
    char* error_out,
    size_t error_out_cap,
    void* userdata,
    struct json_object** result_out);

void ide_ipc_typing_set_handler(IdeIpcTypingHandler handler, void* userdata);
bool ide_ipc_typing_request_json(struct json_object* args,
                                 struct json_object** result_out,
                                 struct json_object** error_out);
bool ide_ipc_typing_request(const IdeIpcTypingRequest* request,
                            struct json_object** result_out,
                            char* error_out,
                            size_t error_out_cap);
void ide_ipc_typing_pump(void);
void ide_ipc_typing_cancel(void);

#endif
