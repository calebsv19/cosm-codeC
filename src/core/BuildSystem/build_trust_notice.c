#include "build_trust_notice.h"

#include <stdio.h>

static const char* safe_text(const char* text, const char* fallback) {
    return (text && text[0]) ? text : fallback;
}

const char* ide_build_trust_action_label(IdeBuildTrustAction action) {
    switch (action) {
        case IDE_BUILD_TRUST_ACTION_BUILD:
            return "Build";
        case IDE_BUILD_TRUST_ACTION_RUN:
            return "Run";
        default:
            return "Command";
    }
}

void ide_build_trust_notice_format(char* out,
                                   size_t out_size,
                                   IdeBuildTrustAction action,
                                   const char* command_source,
                                   const char* working_dir,
                                   const char* command_display) {
    if (!out || out_size == 0) return;

    const char* label = ide_build_trust_action_label(action);
    const char* source = safe_text(command_source, "unknown");
    const char* cwd = safe_text(working_dir, "(unset)");
    const char* command = safe_text(command_display, "(unset)");

    snprintf(out,
             out_size,
             "[Trust] User-triggered trusted workspace action: %s.\n"
             "[Trust] This may execute workspace/configured commands in: %s\n"
             "[Trust] Command source: %s\n"
             "[Trust] Command: %s\n",
             label,
             cwd,
             source,
             command);
}
