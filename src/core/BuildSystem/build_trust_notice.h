#ifndef BUILD_TRUST_NOTICE_H
#define BUILD_TRUST_NOTICE_H

#include <stddef.h>

typedef enum IdeBuildTrustAction {
    IDE_BUILD_TRUST_ACTION_BUILD = 0,
    IDE_BUILD_TRUST_ACTION_RUN = 1
} IdeBuildTrustAction;

const char* ide_build_trust_action_label(IdeBuildTrustAction action);

void ide_build_trust_notice_format(char* out,
                                   size_t out_size,
                                   IdeBuildTrustAction action,
                                   const char* command_source,
                                   const char* working_dir,
                                   const char* command_display);

#endif
