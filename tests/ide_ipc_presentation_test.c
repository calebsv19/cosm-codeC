#include "core/Ipc/ide_ipc_presentation.h"

#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static char shown[IDE_IPC_PRESENTATION_MAX_BYTES + 1];
static atomic_bool handled;

static bool display(const char* text, char* error_out, size_t cap, void* userdata) {
    (void)error_out;
    (void)cap;
    (void)userdata;
    snprintf(shown, sizeof(shown), "%s", text);
    atomic_store(&handled, true);
    return true;
}

typedef struct {
    bool success;
    char error[160];
} RequestResult;

static void* request_main(void* userdata) {
    RequestResult* result = userdata;
    result->success = ide_ipc_presentation_request("[Agent example]\ndecimal result: 2.5\n",
                                                       result->error, sizeof(result->error));
    return NULL;
}

int main(void) {
    char error[160] = {0};
    assert(!ide_ipc_presentation_request("test", error, sizeof(error)));
    assert(strstr(error, "unavailable"));
    ide_ipc_presentation_set_handler(display, NULL);
    assert(!ide_ipc_presentation_request("bad\033terminal", error, sizeof(error)));
    assert(strstr(error, "printable"));
    char too_long[IDE_IPC_PRESENTATION_MAX_BYTES + 2];
    memset(too_long, 'x', sizeof(too_long) - 1);
    too_long[sizeof(too_long) - 1] = '\0';
    assert(!ide_ipc_presentation_request(too_long, error, sizeof(error)));

    RequestResult result = {0};
    pthread_t worker;
    assert(pthread_create(&worker, NULL, request_main, &result) == 0);
    for (int i = 0; i < 1000 && !atomic_load(&handled); ++i) {
        ide_ipc_presentation_pump();
        usleep(1000);
    }
    assert(atomic_load(&handled));
    assert(pthread_join(worker, NULL) == 0);
    assert(result.success);
    assert(strcmp(shown, "[Agent example]\ndecimal result: 2.5\n") == 0);
    ide_ipc_presentation_cancel();
    return 0;
}
