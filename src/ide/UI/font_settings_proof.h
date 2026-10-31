#ifndef IDE_FONT_SETTINGS_PROOF_H
#define IDE_FONT_SETTINGS_PROOF_H
#include <stdbool.h>
/* Called only by the explicit disposable-workspace lifecycle fixture. */
bool ide_font_settings_proof_run(bool reopen);
#endif
