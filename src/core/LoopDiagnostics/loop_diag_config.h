#ifndef LOOP_DIAG_CONFIG_H
#define LOOP_DIAG_CONFIG_H

#include <stdbool.h>

typedef struct {
    bool enabled;
    bool json_output;
    int max_wait_ms_override;
} LoopDiagConfig;

LoopDiagConfig loop_diag_config_from_env(void);

#endif // LOOP_DIAG_CONFIG_H
