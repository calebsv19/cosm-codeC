#ifndef IDE_WINDOW_LIFECYCLE_PROOF_H
#define IDE_WINDOW_LIFECYCLE_PROOF_H
#include <stdbool.h>
void ide_window_proof_tick(bool* running);
void ide_window_proof_presented(void);
int ide_window_proof_exit_code(void);
#endif
