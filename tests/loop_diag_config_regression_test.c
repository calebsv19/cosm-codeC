#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "core/LoopDiagnostics/loop_diag_config.h"

static void clear_diag_env(void) {
    unsetenv("IDE_LOOP_DIAG_LOG");
    unsetenv("IDE_EVENT_DIAG_LOG");
    unsetenv("IDE_LOOP_DIAG_FORMAT");
    unsetenv("IDE_LOOP_DIAG_JSON");
    unsetenv("IDE_LOOP_MAX_WAIT_MS");
}

static void test_defaults_disabled(void) {
    clear_diag_env();
    LoopDiagConfig cfg = loop_diag_config_from_env();
    assert(!cfg.enabled);
    assert(!cfg.json_output);
    assert(cfg.max_wait_ms_override == -1);
}

static void test_json_format_enables_diag(void) {
    clear_diag_env();
    setenv("IDE_LOOP_DIAG_FORMAT", "json", 1);
    LoopDiagConfig cfg = loop_diag_config_from_env();
    assert(cfg.enabled);
    assert(cfg.json_output);
    assert(cfg.max_wait_ms_override == -1);
}

static void test_json_flag_enables_diag(void) {
    clear_diag_env();
    setenv("IDE_LOOP_DIAG_JSON", "1", 1);
    LoopDiagConfig cfg = loop_diag_config_from_env();
    assert(cfg.enabled);
    assert(cfg.json_output);
}

static void test_event_diag_enables_without_json(void) {
    clear_diag_env();
    setenv("IDE_EVENT_DIAG_LOG", "true", 1);
    LoopDiagConfig cfg = loop_diag_config_from_env();
    assert(cfg.enabled);
    assert(!cfg.json_output);
}

static void test_wait_override_bounds(void) {
    clear_diag_env();
    setenv("IDE_LOOP_MAX_WAIT_MS", "200", 1);
    LoopDiagConfig cfg = loop_diag_config_from_env();
    assert(cfg.max_wait_ms_override == 200);

    setenv("IDE_LOOP_MAX_WAIT_MS", "0", 1);
    cfg = loop_diag_config_from_env();
    assert(cfg.max_wait_ms_override == -1);

    setenv("IDE_LOOP_MAX_WAIT_MS", "99999", 1);
    cfg = loop_diag_config_from_env();
    assert(cfg.max_wait_ms_override == -1);
}

int main(void) {
    test_defaults_disabled();
    test_json_format_enables_diag();
    test_json_flag_enables_diag();
    test_event_diag_enables_without_json();
    test_wait_override_bounds();
    clear_diag_env();
    puts("loop_diag_config_regression_test: success");
    return 0;
}
