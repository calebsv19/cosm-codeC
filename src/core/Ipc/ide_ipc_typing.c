#include "core/Ipc/ide_ipc_typing.h"

#include <errno.h>
#include <json-c/json.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "core/Ipc/ide_ipc_server_utils.h"

typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    bool inflight;
    bool pending;
    bool started;
    bool completed;
    bool success;
    uint64_t generation;
    IdeIpcTypingRequest request;
    char* path;
    char* expected_text;
    char* replacement_text;
    char expected_before_fnv64[17];
    char expected_after_fnv64[17];
    char error[256];
    char* result_json;
    IdeIpcTypingHandler handler;
    void* userdata;
} TypingState;

static TypingState g_typing = {
    .lock = PTHREAD_MUTEX_INITIALIZER,
    .cond = PTHREAD_COND_INITIALIZER,
};

static void set_error(char* out, size_t cap, const char* message) {
    if (out && cap) snprintf(out, cap, "%s", message ? message : "Typing failed");
}

static void clear_request_locked(void) {
    free(g_typing.path);
    free(g_typing.expected_text);
    free(g_typing.replacement_text);
    free(g_typing.result_json);
    g_typing.path = NULL;
    g_typing.expected_text = NULL;
    g_typing.replacement_text = NULL;
    g_typing.result_json = NULL;
    memset(&g_typing.request, 0, sizeof(g_typing.request));
}

static bool valid_hash(const char* value) {
    if (!value || strlen(value) != 16) return false;
    return strspn(value, "0123456789abcdef") == 16;
}

static bool valid_text(const char* value, bool allow_empty) {
    if (!value) return false;
    size_t length = strnlen(value, IDE_IPC_TYPING_MAX_TEXT_BYTES + 1);
    if ((!allow_empty && length == 0) || length > IDE_IPC_TYPING_MAX_TEXT_BYTES) return false;
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = (unsigned char)value[i];
        if (c > 127 || (c < 32 && c != '\n' && c != '\t') || c == 127) return false;
    }
    return true;
}

void ide_ipc_typing_set_handler(IdeIpcTypingHandler handler, void* userdata) {
    pthread_mutex_lock(&g_typing.lock);
    g_typing.handler = handler;
    g_typing.userdata = userdata;
    pthread_mutex_unlock(&g_typing.lock);
}

bool ide_ipc_typing_request(const IdeIpcTypingRequest* request,
                            json_object** result_out,
                            char* error_out,
                            size_t error_out_cap) {
    if (result_out) *result_out = NULL;
    if (!request || !request->path || !request->path[0] ||
        !valid_hash(request->expected_before_fnv64) ||
        !valid_hash(request->expected_after_fnv64) ||
        !valid_text(request->expected_text, true) ||
        !valid_text(request->replacement_text, false) ||
        request->start_line < 1 || request->start_column < 1 ||
        request->end_line < request->start_line || request->end_column < 1 ||
        request->cadence_ms < 16 || request->cadence_ms > 500) {
        set_error(error_out, error_out_cap, "Invalid checked typing request");
        return false;
    }

    pthread_mutex_lock(&g_typing.lock);
    if (!g_typing.handler || g_typing.inflight) {
        set_error(error_out, error_out_cap,
                  g_typing.handler ? "Visible typing is busy" : "Visible typing handler unavailable");
        pthread_mutex_unlock(&g_typing.lock);
        return false;
    }

    clear_request_locked();
    g_typing.path = strdup(request->path);
    g_typing.expected_text = strdup(request->expected_text);
    g_typing.replacement_text = strdup(request->replacement_text);
    if (!g_typing.path || !g_typing.expected_text || !g_typing.replacement_text) {
        clear_request_locked();
        set_error(error_out, error_out_cap, "Out of memory preparing visible typing");
        pthread_mutex_unlock(&g_typing.lock);
        return false;
    }
    snprintf(g_typing.expected_before_fnv64, sizeof(g_typing.expected_before_fnv64),
             "%s", request->expected_before_fnv64);
    snprintf(g_typing.expected_after_fnv64, sizeof(g_typing.expected_after_fnv64),
             "%s", request->expected_after_fnv64);
    g_typing.request = *request;
    g_typing.request.path = g_typing.path;
    g_typing.request.expected_text = g_typing.expected_text;
    g_typing.request.replacement_text = g_typing.replacement_text;
    g_typing.request.expected_before_fnv64 = g_typing.expected_before_fnv64;
    g_typing.request.expected_after_fnv64 = g_typing.expected_after_fnv64;
    g_typing.inflight = true;
    g_typing.pending = true;
    g_typing.started = false;
    g_typing.completed = false;
    g_typing.success = false;
    g_typing.error[0] = '\0';
    g_typing.generation++;

    size_t bytes = strlen(g_typing.replacement_text);
    long long estimated_ms = (long long)bytes * request->cadence_ms + 30000LL;
    if (estimated_ms > 600000LL) estimated_ms = 600000LL;
    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += estimated_ms / 1000;
    deadline.tv_nsec += (estimated_ms % 1000) * 1000000LL;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000L;
    }

    while (!g_typing.completed) {
        int rc = pthread_cond_timedwait(&g_typing.cond, &g_typing.lock, &deadline);
        if (rc == ETIMEDOUT) {
            g_typing.pending = false;
            g_typing.inflight = false;
            set_error(error_out, error_out_cap,
                      "Visible typing timed out; the IDE may contain a partial unsaved edit");
            pthread_mutex_unlock(&g_typing.lock);
            return false;
        }
    }

    bool success = g_typing.success;
    if (success && result_out && g_typing.result_json) {
        *result_out = json_tokener_parse(g_typing.result_json);
        if (!*result_out || !json_object_is_type(*result_out, json_type_object)) {
            if (*result_out) json_object_put(*result_out);
            *result_out = NULL;
            success = false;
            set_error(error_out, error_out_cap, "Visible typing returned an invalid result");
        }
    }
    if (!success && (!error_out || !error_out[0])) {
        set_error(error_out, error_out_cap, g_typing.error);
    }
    g_typing.inflight = false;
    clear_request_locked();
    pthread_mutex_unlock(&g_typing.lock);
    return success;
}

static bool get_plain_string(json_object* object, const char* key, const char** value_out) {
    json_object* value = NULL;
    if (!object || !json_object_object_get_ex(object, key, &value) ||
        !value || !json_object_is_type(value, json_type_string)) return false;
    const char* text = json_object_get_string(value);
    if (!text || (size_t)json_object_get_string_len(value) != strlen(text)) return false;
    *value_out = text;
    return true;
}

static bool get_int(json_object* object, const char* key, int* value_out) {
    json_object* value = NULL;
    if (!object || !json_object_object_get_ex(object, key, &value) ||
        !value || !json_object_is_type(value, json_type_int)) return false;
    *value_out = json_object_get_int(value);
    return true;
}

bool ide_ipc_typing_request_json(json_object* args,
                                 json_object** result_out,
                                 json_object** error_out) {
    if (result_out) *result_out = NULL;
    if (error_out) *error_out = NULL;
    IdeIpcTypingRequest request = {0};
    if (!get_plain_string(args, "path", &request.path) ||
        !get_plain_string(args, "expected_before_fnv64", &request.expected_before_fnv64) ||
        !get_plain_string(args, "expected_after_fnv64", &request.expected_after_fnv64) ||
        !get_plain_string(args, "expected_text", &request.expected_text) ||
        !get_plain_string(args, "replacement_text", &request.replacement_text) ||
        !get_int(args, "start_line", &request.start_line) ||
        !get_int(args, "start_column", &request.start_column) ||
        !get_int(args, "end_line", &request.end_line) ||
        !get_int(args, "end_column", &request.end_column) ||
        !get_int(args, "cadence_ms", &request.cadence_ms)) {
        if (error_out) *error_out = ide_ipc_build_error_obj(
            "bad_request", "type requires checked path, hashes, range, text, and cadence", NULL);
        return false;
    }
    char error[256] = {0};
    if (!ide_ipc_typing_request(&request, result_out, error, sizeof(error))) {
        if (error_out) *error_out = ide_ipc_build_error_obj(
            "typing_failed", error[0] ? error : "Visible typing failed", NULL);
        return false;
    }
    return true;
}

void ide_ipc_typing_pump(void) {
    IdeIpcTypingHandler handler = NULL;
    void* userdata = NULL;
    const IdeIpcTypingRequest* request = NULL;
    uint64_t generation = 0;
    bool begin = false;

    pthread_mutex_lock(&g_typing.lock);
    if (g_typing.pending && g_typing.inflight) {
        handler = g_typing.handler;
        userdata = g_typing.userdata;
        request = &g_typing.request;
        generation = g_typing.generation;
        begin = !g_typing.started;
        g_typing.started = true;
    }
    pthread_mutex_unlock(&g_typing.lock);
    if (!handler || !request) return;

    char error[256] = {0};
    json_object* result = NULL;
    IdeIpcTypingStepResult step = handler(request, begin, error, sizeof(error), userdata, &result);
    if (step == IDE_IPC_TYPING_STEP_CONTINUE) {
        if (result) json_object_put(result);
        return;
    }

    pthread_mutex_lock(&g_typing.lock);
    if (g_typing.inflight && g_typing.generation == generation) {
        g_typing.pending = false;
        g_typing.completed = true;
        g_typing.success = (step == IDE_IPC_TYPING_STEP_COMPLETE);
        set_error(g_typing.error, sizeof(g_typing.error), error);
        if (result) {
            g_typing.result_json = strdup(
                json_object_to_json_string_ext(result, JSON_C_TO_STRING_PLAIN));
        }
        pthread_cond_broadcast(&g_typing.cond);
    }
    pthread_mutex_unlock(&g_typing.lock);
    if (result) json_object_put(result);
}

void ide_ipc_typing_cancel(void) {
    pthread_mutex_lock(&g_typing.lock);
    if (g_typing.inflight) {
        g_typing.pending = false;
        g_typing.completed = true;
        g_typing.success = false;
        set_error(g_typing.error, sizeof(g_typing.error),
                  "IDE session stopped during visible typing; inspect for partial unsaved edits");
        pthread_cond_broadcast(&g_typing.cond);
    }
    pthread_mutex_unlock(&g_typing.lock);
}
