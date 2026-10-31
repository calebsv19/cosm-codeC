/* Real SDL_ttf/CoreText production loaders; only resource lookup is injected. */
#include "engine/Render/render_font.h"
#include "engine/Render/render_pipeline.h"
#include "app/GlobalInfo/font_settings_runtime.h"
#include "app/GlobalInfo/workspace_authoring_session_adapter.h"
#include "ide/UI/ui_state.h"
#include "engine/Render/render_text_helpers.h"
#include "ide/UI/shared_theme_font_adapter.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

RenderContext* getRenderContext(void) { return NULL; }

static unsigned saves, notifications;
static int saved_zoom;
static char saved_preset[64];
static UIState ui = {.toolPanelVisible=true, .controlPanelVisible=true, .terminalVisible=true};
UIState* getUIState(void) { return &ui; }
void render_text_cache_shutdown(void) {}
void terminal_notify_font_metrics_changed(void) { ++notifications; }
void ide_refresh_live_theme(void) {}
void requestFullRedraw(uint32_t reasons) { (void)reasons; }
void saveFontPresetPreference(const char* preset) { ++saves; snprintf(saved_preset,sizeof(saved_preset),"%s",preset); }
void saveFontZoomStepPreference(int step) { ++saves; saved_zoom=step; }
void saveThemePresetPreference(const char* preset) { (void)preset; }
void saveWorkspaceAuthoringPresentationPreference(int a, int b, int c, int d) {
    (void)a; (void)b; (void)c; (void)d;
}
bool ide_workspace_authoring_host_active(const IDEWorkspaceAuthoringHost* host) { return host && host->active; }
IDEWorkspaceAuthoringProfileResult ide_workspace_authoring_profile_import_file(const char* path, IDEWorkspaceAuthoringProjection* out) {
    (void)path; (void)out; return IDE_WORKSPACE_AUTHORING_PROFILE_ERR_IO;
}

static int unavailable;
static char source_root[4096];
bool ide_runtime_paths_init(const char* argv0) { (void)argv0; return true; }
bool ide_runtime_probe_resource_path(const char* path, char* out, size_t cap) {
    if (unavailable) snprintf(out, cap, "/nonexistent-codework-font-fixture.ttf");
    else snprintf(out, cap, "%s/%s", source_root, path);
    return true;
}

static void verify_fonts(void) {
    for (int i = 0; i < CORE_FONT_TEXT_SIZE_COUNT; ++i) {
        TTF_Font* font = getUIFontByTier((CoreFontTextSizeTier)i);
        int w = 0, h = 0;
        assert(font && TTF_SizeUTF8(font, "Wim 0123", &w, &h) == 0 && w > 0 && h > 0);
    }
    assert(getTerminalFont() && TTF_FontHeight(getTerminalFont()) > 0);
    const float scales[] = {1, 1.25f, 1.5f, 2};
    for (unsigned i = 0; i < sizeof(scales)/sizeof(scales[0]); ++i) {
        float raster_scale = 0;
        TTF_Font* raster = getRasterizedFontForScale(getActiveFont(), scales[i], &raster_scale);
        SDL_Surface* surface = TTF_RenderUTF8_Blended(raster, "Wim 0123", (SDL_Color){255,255,255,255});
        assert(surface && surface->w > 0 && surface->h > 0 && raster_scale >= 1);
        SDL_FreeSurface(surface);
    }
}

int main(void) {
    assert(getcwd(source_root, sizeof(source_root)));
    setenv("IDE_USE_SHARED_FONT", "1", 1);
    setenv("IDE_FONT_PRESET", "ide", 1);
    assert(initFontSystem());
    verify_fonts();
    TTF_Font* previous = getActiveFont();
    TTF_Font* terminal = getTerminalFont();
    int point = getUIFontPointSizeByTier(CORE_FONT_TEXT_SIZE_BASIC);
    unavailable = 1;
    assert(!reloadFontSystem());
    assert(getActiveFont() == previous && getTerminalFont() == terminal);
    assert(getUIFontPointSizeByTier(CORE_FONT_TEXT_SIZE_BASIC) == point);
    unavailable = 0;
    verify_fonts();

    /* Same buffer, same length/first 64/final byte, different measured suffix. */
    char text[161];
    memset(text, 'i', 160); text[160] = 0;
    int limit = getTextWidthWithFont(text, getActiveFont());
    assert(getTextClampedLength(text, limit) == 160);
    memset(text + 64, 'W', 95);
    assert(getTextWidthWithFont(text, getActiveFont()) > limit);
    assert(getTextClampedLength(text, limit) < 160);

    /* Legacy selection must replace the aliased basic tier, not leave it freed. */
    assert(loadFontByID(FONT_LATO_REGULAR));
    assert(getUIFontByTier(CORE_FONT_TEXT_SIZE_BASIC) == getActiveFont());
    verify_fonts();
    previous = getActiveFont();
    unavailable = 1;
    assert(!loadFontByID(FONT_MONTSERRAT_BOLD));
    assert(getActiveFont() == previous);
    unavailable = 0;
    for (int i = 0; i < 24; ++i) {
        assert(ide_shared_font_set_preset(i % 2 ? "ide" : "daw_default"));
        (void)ide_shared_font_set_zoom_step(i % 5 - 2);
        assert(reloadFontSystem());
        verify_fonts();
    }
    /* Actual settings and authoring session hooks; preference sinks count writes. */
    assert(ide_font_settings_apply("ide", 0, true));
    assert(saved_zoom==0 && strcmp(saved_preset,"ide")==0);
    unsigned writes=saves, metric_updates=notifications;
    previous=getActiveFont(); terminal=getTerminalFont();
    unavailable=1;
    assert(!ide_font_settings_apply("daw_default", 2, true));
    char preset[64]; assert(ide_shared_font_current_preset(preset,sizeof(preset)));
    assert(strcmp(preset,"ide")==0 && ide_shared_font_zoom_step()==0);
    assert(getActiveFont()==previous && getTerminalFont()==terminal);
    assert(saves==writes && notifications==metric_updates);
    unavailable=0;
    IDEWorkspaceAuthoringHost host={0};
    ide_workspace_authoring_session_adapter_reset(&host);
    assert(ide_workspace_authoring_session_adapter_enter(&host)==CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_NONE);
    assert(!beginFontPreview()); /* A second writer cannot replace the entry pin. */
    assert(ide_font_settings_apply("daw_default", 2, false));
    assert(getActiveFont()!=previous && saves==writes);
    assert(ide_font_settings_apply("ide", -1, false));
    unavailable=1; /* Cancel must restore exact entry handles without reopening files. */
    assert(ide_workspace_authoring_session_adapter_cancel(&host)==CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_NOOP);
    assert(!host.active && getActiveFont()==previous && getTerminalFont()==terminal);
    assert(ide_shared_font_zoom_step()==0 && saves==writes);
    assert(ide_shared_font_current_preset(preset,sizeof(preset)) && strcmp(preset,"ide")==0);
    unavailable=0; verify_fonts();
    assert(ide_workspace_authoring_session_adapter_enter(&host)==CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_NONE);
    assert(ide_font_settings_apply("daw_default", 1, false));
    previous=getActiveFont();
    assert(ide_workspace_authoring_session_adapter_apply(&host)==CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_APPLIED);
    assert(!host.active && saves==writes+2 && saved_zoom==1 && strcmp(saved_preset,"daw_default")==0);
    assert(getActiveFont()==previous); verify_fonts();
    /* No-change sessions, legacy partially shared tiers, and shutdown during preview. */
    assert(beginFontPreview()); acceptFontPreview(); verify_fonts();
    assert(beginFontPreview()); assert(loadFontByID(FONT_LATO_BOLD));
    assert(cancelFontPreview()); verify_fonts();
    assert(beginFontPreview()); assert(ide_font_settings_apply("ide", 0, false));
    shutdownFontSystem();
    assert(!getActiveFont() && !getTerminalFont());
    puts("render_font_lifecycle_test: PASS (real fonts, rollback, tiers, clamp mutation, presets/zoom, settings failure/no-save, authoring Apply/Cancel exact handles, 1/1.25/1.5/2 raster scales)");
    return 0;
}
