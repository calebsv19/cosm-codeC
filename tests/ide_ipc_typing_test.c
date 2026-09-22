#include "core/Ipc/ide_ipc_typing.h"

#include <assert.h>
#include <json-c/json.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef struct {
    bool success;
    json_object* result;
    char error[256];
} RequestResult;

static int step_count;

static IdeIpcTypingStepResult step_handler(const IdeIpcTypingRequest* request,
                                           bool begin,
                                           char* error_out,
                                           size_t error_out_cap,
                                           void* userdata,
                                           json_object** result_out) {
    (void)error_out;
    (void)error_out_cap;
    (void)userdata;
    assert(strcmp(request->path, "lesson.c") == 0);
    if (begin) assert(step_count == 0);
    step_count++;
    if (step_count < 4) return IDE_IPC_TYPING_STEP_CONTINUE;
    *result_out = json_object_new_object();
    json_object_object_add(*result_out, "mutation_steps", json_object_new_int(3));
    return IDE_IPC_TYPING_STEP_COMPLETE;
}

static void* request_thread(void* context) {
    RequestResult* result = context;
    IdeIpcTypingRequest request = {
        .path = "lesson.c",
        .expected_before_fnv64 = "1111111111111111",
        .expected_after_fnv64 = "2222222222222222",
        .expected_text = "TODO",
        .replacement_text = "int x = 1;\n",
        .start_line = 1,
        .start_column = 1,
        .end_line = 1,
        .end_column = 5,
        .cadence_ms = 20,
    };
    result->success = ide_ipc_typing_request(&request, &result->result,
                                              result->error, sizeof(result->error));
    return NULL;
}

int main(void) {
    char error[256] = {0};
    IdeIpcTypingRequest invalid = {0};
    assert(!ide_ipc_typing_request(&invalid, NULL, error, sizeof(error)));

    ide_ipc_typing_set_handler(step_handler, NULL);
    RequestResult result = {0};
    pthread_t thread;
    assert(pthread_create(&thread, NULL, request_thread, &result) == 0);
    struct timespec pause = {.tv_sec = 0, .tv_nsec = 1000000L};
    while (step_count < 4) {
        ide_ipc_typing_pump();
        nanosleep(&pause, NULL);
    }
    pthread_join(thread, NULL);
    assert(result.success);
    assert(result.result);
    json_object* steps = NULL;
    assert(json_object_object_get_ex(result.result, "mutation_steps", &steps));
    assert(json_object_get_int(steps) == 3);
    json_object_put(result.result);
    ide_ipc_typing_cancel();
    puts("ide_ipc_typing_test: ok");
    return 0;
}
