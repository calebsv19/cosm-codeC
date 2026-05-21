#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "app/GlobalInfo/workspace_startup_policy.h"

static void test_build_default_root_prefers_override(void) {
    char out[1024];
    assert(ide_workspace_startup_build_default_root("/tmp/override",
                                                    "/Users/test",
                                                    "/tmp/cwd",
                                                    out,
                                                    sizeof(out)));
    assert(strcmp(out, "/tmp/override") == 0);
}

static void test_build_default_root_uses_home_then_cwd(void) {
    char out[1024];
    assert(ide_workspace_startup_build_default_root(NULL,
                                                    "/Users/tester",
                                                    "/tmp/cwd",
                                                    out,
                                                    sizeof(out)));
    assert(strcmp(out, "/Users/tester/Desktop/CodeWork") == 0);

    assert(ide_workspace_startup_build_default_root(NULL,
                                                    NULL,
                                                    "/tmp/cwd",
                                                    out,
                                                    sizeof(out)));
    assert(strcmp(out, "/tmp/cwd") == 0);
}

static void test_select_root_prefers_valid_stored_path(void) {
    char out[1024];
    bool used_fallback = false;
    assert(ide_workspace_startup_select_root("/tmp/stored",
                                             true,
                                             "/tmp/default",
                                             true,
                                             out,
                                             sizeof(out),
                                             &used_fallback));
    assert(strcmp(out, "/tmp/stored") == 0);
    assert(!used_fallback);
}

static void test_select_root_falls_back_to_default_when_stored_missing(void) {
    char out[1024];
    bool used_fallback = false;
    assert(ide_workspace_startup_select_root("/tmp/stored-missing",
                                             false,
                                             "/tmp/default",
                                             true,
                                             out,
                                             sizeof(out),
                                             &used_fallback));
    assert(strcmp(out, "/tmp/default") == 0);
    assert(used_fallback);
}

static void test_select_root_fails_when_nothing_valid_exists(void) {
    char out[1024];
    bool used_fallback = false;
    assert(!ide_workspace_startup_select_root("/tmp/stored-missing",
                                              false,
                                              "/tmp/default-missing",
                                              false,
                                              out,
                                              sizeof(out),
                                              &used_fallback));
}

int main(void) {
    test_build_default_root_prefers_override();
    test_build_default_root_uses_home_then_cwd();
    test_select_root_prefers_valid_stored_path();
    test_select_root_falls_back_to_default_when_stored_missing();
    test_select_root_fails_when_nothing_valid_exists();
    puts("workspace_startup_policy_test: success");
    return 0;
}
