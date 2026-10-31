#include "app/GlobalInfo/font_settings_runtime.h"
#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/workspace_prefs.h"
#include "engine/Render/render_font.h"
#include "engine/Render/render_helpers.h"
#include "ide/UI/shared_theme_font_adapter.h"
#include "ide/Panes/Terminal/terminal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void ide_font_settings_persist_current(void) {
    char preset[64], zoom[16];
    int step = ide_shared_font_zoom_step();
    if (ide_shared_font_current_preset(preset, sizeof(preset))) saveFontPresetPreference(preset);
    snprintf(zoom, sizeof(zoom), "%d", step);
    setenv("IDE_FONT_ZOOM_STEP", zoom, 1);
    saveFontZoomStepPreference(step);
}

bool ide_font_settings_apply(const char* preset, int zoom_step, bool persist) {
    char previous[64];
    int old_step = ide_shared_font_zoom_step();
    if (!preset || !ide_shared_font_current_preset(previous, sizeof(previous))) return false;
    if (!ide_shared_font_set_preset(preset)) return false;
    (void)ide_shared_font_set_zoom_step(zoom_step);
    bool changed = strcmp(previous, preset) != 0 || old_step != ide_shared_font_zoom_step();
    if (changed) {
        /* Texture keys contain font pointers; retire before releasing any handle. */
        render_text_cache_shutdown();
        if (!reloadFontSystem()) {
            (void)ide_shared_font_set_preset(previous);
            (void)ide_shared_font_set_zoom_step(old_step);
            requestFullRedraw(RENDER_INVALIDATION_CONTENT | RENDER_INVALIDATION_OVERLAY);
            return false;
        }
        terminal_notify_font_metrics_changed();
        requestFullRedraw(RENDER_INVALIDATION_LAYOUT | RENDER_INVALIDATION_RESIZE |
                          RENDER_INVALIDATION_CONTENT | RENDER_INVALIDATION_BACKGROUND);
    }
    if (persist) ide_font_settings_persist_current();
    return true;
}
