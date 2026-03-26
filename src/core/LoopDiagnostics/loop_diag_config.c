#include "core/LoopDiagnostics/loop_diag_config.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

static bool env_truthy(const char* value) {
    return value && value[0] &&
           (strcmp(value, "1") == 0 || strcasecmp(value, "true") == 0);
}

LoopDiagConfig loop_diag_config_from_env(void) {
    LoopDiagConfig cfg = {0};
    cfg.max_wait_ms_override = -1;

    const char* diag_env = getenv("IDE_LOOP_DIAG_LOG");
    if (env_truthy(diag_env)) {
        cfg.enabled = true;
    }

    const char* format_env = getenv("IDE_LOOP_DIAG_FORMAT");
    if (format_env && format_env[0] && strcasecmp(format_env, "json") == 0) {
        cfg.json_output = true;
        cfg.enabled = true;
    }

    const char* json_env = getenv("IDE_LOOP_DIAG_JSON");
    if (env_truthy(json_env)) {
        cfg.json_output = true;
        cfg.enabled = true;
    }

    const char* event_diag_env = getenv("IDE_EVENT_DIAG_LOG");
    if (env_truthy(event_diag_env)) {
        cfg.enabled = true;
    }

    const char* wait_env = getenv("IDE_LOOP_MAX_WAIT_MS");
    if (wait_env && wait_env[0]) {
        char* end = NULL;
        long value = strtol(wait_env, &end, 10);
        if (end != wait_env && value >= 1 && value <= 5000) {
            cfg.max_wait_ms_override = (int)value;
        }
    }

    return cfg;
}
