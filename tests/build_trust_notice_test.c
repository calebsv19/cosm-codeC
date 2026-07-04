#include "core/BuildSystem/build_trust_notice.h"

#include <stdio.h>
#include <string.h>

static int expect_contains(const char* haystack, const char* needle, const char* label) {
    if (!haystack || !needle || !strstr(haystack, needle)) {
        fprintf(stderr, "missing %s: %s\n", label, needle ? needle : "(null)");
        return 1;
    }
    return 0;
}

int main(void) {
    char notice[1024];

    ide_build_trust_notice_format(notice,
                                  sizeof(notice),
                                  IDE_BUILD_TRUST_ACTION_BUILD,
                                  "workspace_config",
                                  "/tmp/workspace",
                                  "make all");
    if (expect_contains(notice, "User-triggered trusted workspace action: Build.", "build action")) return 1;
    if (expect_contains(notice, "may execute workspace/configured commands", "trust wording")) return 1;
    if (expect_contains(notice, "Command source: workspace_config", "build source")) return 1;
    if (expect_contains(notice, "Command: make all", "build command")) return 1;

    ide_build_trust_notice_format(notice,
                                  sizeof(notice),
                                  IDE_BUILD_TRUST_ACTION_RUN,
                                  "selected_executable",
                                  "/tmp/workspace",
                                  "/tmp/workspace/build/app");
    if (expect_contains(notice, "User-triggered trusted workspace action: Run.", "run action")) return 1;
    if (expect_contains(notice, "Command source: selected_executable", "run source")) return 1;
    if (expect_contains(notice, "Command: /tmp/workspace/build/app", "run target")) return 1;

    ide_build_trust_notice_format(notice,
                                  sizeof(notice),
                                  IDE_BUILD_TRUST_ACTION_BUILD,
                                  NULL,
                                  NULL,
                                  NULL);
    if (expect_contains(notice, "Command source: unknown", "fallback source")) return 1;
    if (expect_contains(notice, "Command: (unset)", "fallback command")) return 1;

    return 0;
}
