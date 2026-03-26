#include "runtime_startup_defaults.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static int parse_bool_env(const char* value, int* out) {
    if (!value || !value[0] || !out) return 0;
    if (strcmp(value, "1") == 0 ||
        strcasecmp(value, "true") == 0 ||
        strcasecmp(value, "yes") == 0 ||
        strcasecmp(value, "on") == 0) {
        *out = 1;
        return 1;
    }
    if (strcmp(value, "0") == 0 ||
        strcasecmp(value, "false") == 0 ||
        strcasecmp(value, "no") == 0 ||
        strcasecmp(value, "off") == 0) {
        *out = 0;
        return 1;
    }
    return 0;
}

static int defaults_debug_enabled(void) {
    int enabled = 0;
    if (parse_bool_env(getenv("IDE_STARTUP_DEFAULTS_DEBUG"), &enabled)) {
        return enabled;
    }
    return 0;
}

static void apply_default_if_unset(const char* key, const char* value) {
    const char* current = getenv(key);
    if (current && current[0]) {
        return;
    }
    setenv(key, value, 0);
    if (defaults_debug_enabled()) {
        fprintf(stderr, "[StartupDefaults] %s=%s\n", key, value);
    }
}

void ide_apply_runtime_startup_defaults(void) {
    apply_default_if_unset("IDE_USE_SHARED_THEME_FONT", "1");
    apply_default_if_unset("IDE_USE_SHARED_THEME", "1");
    apply_default_if_unset("IDE_USE_SHARED_FONT", "1");
    apply_default_if_unset("IDE_THEME_PRESET", "ide_gray");
    apply_default_if_unset("IDE_FONT_PRESET", "ide");
}
