#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/GlobalInfo/runtime_startup_defaults.h"

static void clear_defaults_env(void) {
    unsetenv("IDE_USE_SHARED_THEME_FONT");
    unsetenv("IDE_USE_SHARED_THEME");
    unsetenv("IDE_USE_SHARED_FONT");
    unsetenv("IDE_THEME_PRESET");
    unsetenv("IDE_FONT_PRESET");
    unsetenv("IDE_STARTUP_DEFAULTS_DEBUG");
}

static void test_sets_defaults_when_unset(void) {
    clear_defaults_env();
    ide_apply_runtime_startup_defaults();
    assert(strcmp(getenv("IDE_USE_SHARED_THEME_FONT"), "1") == 0);
    assert(strcmp(getenv("IDE_USE_SHARED_THEME"), "1") == 0);
    assert(strcmp(getenv("IDE_USE_SHARED_FONT"), "1") == 0);
    assert(strcmp(getenv("IDE_THEME_PRESET"), "ide_gray") == 0);
    assert(strcmp(getenv("IDE_FONT_PRESET"), "ide") == 0);
}

static void test_preserves_existing_values(void) {
    clear_defaults_env();
    setenv("IDE_USE_SHARED_THEME", "0", 1);
    setenv("IDE_THEME_PRESET", "soft_light", 1);
    ide_apply_runtime_startup_defaults();
    assert(strcmp(getenv("IDE_USE_SHARED_THEME"), "0") == 0);
    assert(strcmp(getenv("IDE_THEME_PRESET"), "soft_light") == 0);
    assert(strcmp(getenv("IDE_USE_SHARED_THEME_FONT"), "1") == 0);
}

int main(void) {
    test_sets_defaults_when_unset();
    test_preserves_existing_values();
    clear_defaults_env();
    puts("runtime_startup_defaults_test: success");
    return 0;
}
