#include "ide/UI/font_settings_proof.h"
#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/workspace_prefs.h"
#include "app/GlobalInfo/workspace_authoring_session_adapter.h"
#include "core/InputManager/input_keyboard.h"
#include "engine/Render/render_font.h"
#include "engine/Render/render_pipeline.h"
#include "ide/UI/shared_theme_font_adapter.h"
#include "ide/Panes/ControlPanel/control_panel.h"
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr,"IDE_FONT_SETTINGS_PROOF fail line=%d condition=%s\n",__LINE__,#condition); return false; } } while (0)
static void shortcut(SDL_Keycode key) {
    SDL_Event e={0}; e.type=SDL_KEYDOWN; e.key.keysym.sym=key; e.key.keysym.mod=KMOD_CTRL;
    handleKeyboardInput(&e,NULL,NULL,NULL);
}
static void authoring_key(IDEWorkspaceAuthoringHost* host, SDL_Keycode key, SDL_Keymod mod) {
    SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=key;e.key.keysym.mod=mod;
    (void)ide_workspace_authoring_host_handle_sdl_event(host,&e,false);
    e.type=SDL_KEYUP;(void)ide_workspace_authoring_host_handle_sdl_event(host,&e,false);
}
static bool enter(IDEWorkspaceAuthoringHost* host) {
    authoring_key(host,SDLK_c,KMOD_ALT);authoring_key(host,SDLK_v,KMOD_ALT);
    if (!host->active) return false;
    return ide_workspace_authoring_host_apply_overlay_button(host,KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_MODE);
}
static bool click_font(IDEWorkspaceAuthoringHost* host, bool size) {
    KitRenderContext ctx={0};KitWorkspaceAuthoringFontThemeLayout layout;
    CHECK(kit_render_context_init(&ctx,KIT_RENDER_BACKEND_NULL,CORE_THEME_PRESET_IDE_GRAY,CORE_FONT_PRESET_IDE).code==CORE_OK);
    (void)kit_render_set_text_zoom_step(&ctx,ide_shared_font_zoom_step());
    CHECK(kit_workspace_authoring_ui_font_theme_build_layout(&ctx,host->viewport_width,host->viewport_height,&layout));
    KitRenderRect rect=size ? layout.text_size_inc_button : layout.font_preset_buttons[0];
    kit_render_context_shutdown(&ctx);
    SDL_Event e={0};e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;
    e.button.x=(int)(rect.x+rect.width/2);e.button.y=(int)(rect.y+rect.height/2);
    return ide_workspace_authoring_host_handle_sdl_event(host,&e,false);
}
bool ide_font_settings_proof_run(bool reopen) {
    int saved=0;char preset[64];
    CHECK(ide_shared_font_current_preset(preset,sizeof(preset)));
    if (reopen) {
        CHECK(strcmp(preset,"daw_default")==0 && ide_shared_font_zoom_step()==2);
        CHECK(loadFontZoomStepPreference(&saved) && saved==2);
        CHECK(loadFontPresetPreference() && strcmp(loadFontPresetPreference(),"daw_default")==0);
        puts("IDE_FONT_SETTINGS_PROOF reopen=pass preset=daw_default zoom=2");return true;
    }
    CHECK(strcmp(preset,"ide")==0 && ide_shared_font_zoom_step()==0);
    IDEWorkspaceAuthoringHost* host=&getCoreState()->workspaceAuthoring;
    RenderContext* render=getRenderContext();
    CHECK(render);
    ide_workspace_authoring_host_set_viewport(host, render->width, render->height);
    shortcut(SDLK_EQUALS); CHECK(ide_shared_font_zoom_step()==1);
    CHECK(loadFontZoomStepPreference(&saved) && saved==1);
    TTF_Font* entry=getActiveFont();
    CHECK(enter(host));CHECK(click_font(host,false));CHECK(click_font(host,true));
    CHECK(ide_shared_font_current_preset(preset,sizeof(preset)) && strcmp(preset,"daw_default")==0);
    CHECK(ide_shared_font_zoom_step()==2);
    CHECK(loadFontZoomStepPreference(&saved) && saved==1);
    CHECK(loadFontPresetPreference() && strcmp(loadFontPresetPreference(),"ide")==0);
    authoring_key(host,SDLK_ESCAPE,KMOD_NONE);
    CHECK(!host->active && getActiveFont()==entry && ide_shared_font_zoom_step()==1);
    CHECK(ide_shared_font_current_preset(preset,sizeof(preset)) && strcmp(preset,"ide")==0);
    UIPane* previous_owner=getCoreState()->focusedPane;
    getCoreState()->focusedPane=getUIState()->controlPanel;
    control_panel_set_search_focused(true);
    shortcut(SDLK_MINUS);CHECK(ide_shared_font_zoom_step()==1);
    control_panel_set_search_focused(false);
    getCoreState()->focusedPane=previous_owner;
    shortcut(SDLK_0);CHECK(ide_shared_font_zoom_step()==0);
    CHECK(loadFontZoomStepPreference(&saved) && saved==0);
    shortcut(SDLK_EQUALS);CHECK(ide_shared_font_zoom_step()==1);
    CHECK(enter(host));CHECK(click_font(host,false));CHECK(click_font(host,true));
    authoring_key(host,SDLK_RETURN,KMOD_NONE);
    CHECK(!host->active && ide_shared_font_zoom_step()==2);
    CHECK(loadFontZoomStepPreference(&saved) && saved==2);
    CHECK(loadFontPresetPreference() && strcmp(loadFontPresetPreference(),"daw_default")==0);
    puts("IDE_FONT_SETTINGS_PROOF status=pass shortcuts=plus/minus/reset field-owner=pass preview-no-save=pass cancel-exact-handles=pass apply-persistence=pass");
    return true;
}
