#ifndef IDE_IDE_APP_MAIN_H
#define IDE_IDE_APP_MAIN_H

#include <stdbool.h>

bool ide_app_bootstrap(void);
bool ide_app_config_load(void);
bool ide_app_state_seed(void);
bool ide_app_subsystems_init(void);
bool ide_runtime_start(void);
void ide_app_set_legacy_entry(int (*legacy_entry)(int argc, char **argv));
int ide_app_run_loop(void);
void ide_app_shutdown(void);

int ide_app_main(int argc, char **argv);
int ide_app_main_legacy(int argc, char **argv);

#endif
