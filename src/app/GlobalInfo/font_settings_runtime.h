#ifndef IDE_FONT_SETTINGS_RUNTIME_H
#define IDE_FONT_SETTINGS_RUNTIME_H
#include <stdbool.h>
/* Existing adapter state is committed only after its actual font load succeeds. */
bool ide_font_settings_apply(const char* preset, int zoom_step, bool persist);
void ide_font_settings_persist_current(void);
#endif
