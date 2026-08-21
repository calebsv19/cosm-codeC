#include "app/GlobalInfo/workspace_authoring_host.h"
#include "app/GlobalInfo/workspace_authoring_presentation_ui.h"
#include "app/GlobalInfo/workspace_authoring_profile_path.h"
#include "app/GlobalInfo/workspace_authoring_session_adapter.h"

#include <stdlib.h>
#include <string.h>

#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/workspace_prefs.h"
#include "engine/Render/render_font.h"
#include "engine/Render/render_helpers.h"
#include "ide/Panes/Terminal/terminal.h"
#include "ide/UI/layout.h"
#include "ide/UI/shared_theme_font_adapter.h"

static void authoring_refresh_after_font_theme_change(uint32_t reason_bits) {
    render_text_cache_shutdown();
    (void)reloadFontSystem();
    terminal_notify_font_metrics_changed();
    ide_refresh_live_theme();
    requestFullRedraw(reason_bits);
}

static void authoring_set_status(IDEWorkspaceAuthoringHost *host, const char *status) {
    if (!host) return;
    if (!status) {
        host->status_text[0] = '\0';
        return;
    }
    strncpy(host->status_text, status, sizeof(host->status_text) - 1u);
    host->status_text[sizeof(host->status_text) - 1u] = '\0';
}

static void authoring_clear_event_flags(IDEWorkspaceAuthoringHost *host) {
    if (!host) return;
    host->last_event_consumed = 0u;
    host->last_event_entered = 0u;
    host->last_event_exited = 0u;
    host->last_event_accepted = 0u;
    host->last_event_canceled = 0u;
}

static uint32_t authoring_mod_bits(SDL_Keymod mods) {
    uint32_t bits = 0u;
    if ((mods & KMOD_SHIFT) != 0) bits |= KIT_WORKSPACE_AUTHORING_MOD_SHIFT;
    if ((mods & KMOD_ALT) != 0) bits |= KIT_WORKSPACE_AUTHORING_MOD_ALT;
    if ((mods & KMOD_CTRL) != 0) bits |= KIT_WORKSPACE_AUTHORING_MOD_CTRL;
    if ((mods & KMOD_GUI) != 0) bits |= KIT_WORKSPACE_AUTHORING_MOD_GUI;
    return bits;
}

static KitWorkspaceAuthoringKey authoring_key_from_sdl_keysym(const SDL_Keysym *keysym) {
    if (!keysym) return KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
    switch (keysym->scancode) {
        case SDL_SCANCODE_C: return KIT_WORKSPACE_AUTHORING_KEY_C;
        case SDL_SCANCODE_V: return KIT_WORKSPACE_AUTHORING_KEY_V;
        default: break;
    }
    switch (keysym->sym) {
        case SDLK_TAB: return KIT_WORKSPACE_AUTHORING_KEY_TAB;
        case SDLK_RETURN:
        case SDLK_KP_ENTER: return KIT_WORKSPACE_AUTHORING_KEY_ENTER;
        case SDLK_ESCAPE: return KIT_WORKSPACE_AUTHORING_KEY_ESCAPE;
        case SDLK_c: return KIT_WORKSPACE_AUTHORING_KEY_C;
        case SDLK_v: return KIT_WORKSPACE_AUTHORING_KEY_V;
        default: return KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
    }
}

static void authoring_note_consumed(IDEWorkspaceAuthoringHost *host, bool runtime_event) {
    if (!host) return;
    host->last_event_consumed = 1u;
    host->consumed_event_count += 1u;
    if (runtime_event) {
        host->captured_runtime_event_count += 1u;
    }
}

static void authoring_cycle_overlay(IDEWorkspaceAuthoringHost *host) {
    if (!host || !host->active) return;
    host->overlay_mode = host->overlay_mode == IDE_WORKSPACE_AUTHORING_OVERLAY_PANES
                             ? IDE_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME
                             : IDE_WORKSPACE_AUTHORING_OVERLAY_PANES;
    host->overlay_cycle_count += 1u;
    authoring_set_status(host,
                         host->overlay_mode == IDE_WORKSPACE_AUTHORING_OVERLAY_PANES
                             ? "Pane authoring overlay."
                             : "Font/Theme overlay.");
}

void ide_workspace_authoring_host_reset(IDEWorkspaceAuthoringHost *host) {
    if (!host) return;
    memset(host, 0, sizeof(*host));
    host->entry_chord_armed_key = KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
    host->overlay_mode = IDE_WORKSPACE_AUTHORING_OVERLAY_PANES;
    ide_workspace_authoring_session_adapter_reset(host);
}

void ide_workspace_authoring_host_set_viewport(IDEWorkspaceAuthoringHost *host,
                                               uint32_t width,
                                               uint32_t height) {
    if (!host) return;
    host->viewport_width = width;
    host->viewport_height = height;
}

bool ide_workspace_authoring_host_active(const IDEWorkspaceAuthoringHost *host) {
    return host && core_workspace_authoring_session_authoring_active(&host->session);
}

bool ide_workspace_authoring_host_pane_overlay_active(const IDEWorkspaceAuthoringHost *host) {
    return ide_workspace_authoring_host_active(host) &&
           host->overlay_mode == IDE_WORKSPACE_AUTHORING_OVERLAY_PANES;
}

bool ide_workspace_authoring_host_font_theme_overlay_active(const IDEWorkspaceAuthoringHost *host) {
    return ide_workspace_authoring_host_active(host) &&
           host->overlay_mode == IDE_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME;
}

bool ide_workspace_authoring_host_apply_overlay_button(
    IDEWorkspaceAuthoringHost *host,
    KitWorkspaceAuthoringOverlayButtonId button_id) {
    if (!host || !host->active) return false;
    authoring_clear_event_flags(host);
    switch (button_id) {
        case KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_MODE:
            authoring_cycle_overlay(host);
            return true;
        case KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_APPLY:
            (void)ide_workspace_authoring_session_adapter_apply(host);
            return true;
        case KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_CANCEL:
            (void)ide_workspace_authoring_session_adapter_cancel(host);
            return true;
        case KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_ADD:
            host->add_stub_count += 1u;
            authoring_set_status(host, "Pane/module authoring is unavailable for this IDE session.");
            return true;
        case KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_NONE:
        default:
            return false;
    }
}

bool ide_workspace_authoring_host_toggle_presentation_field(IDEWorkspaceAuthoringHost *host,
                                                            unsigned int field_index) {
    IDEWorkspaceAuthoringProjection draft;
    if (!host || !host->active || !host->presentation_draft_ready) return false;
    draft = host->draft_presentation;
    switch (field_index) {
        case 1u: draft.tool_panel_visible = !draft.tool_panel_visible; break;
        case 2u: draft.control_panel_visible = !draft.control_panel_visible; break;
        case 3u: draft.terminal_visible = !draft.terminal_visible; break;
        default: return false;
    }
    return ide_workspace_authoring_session_adapter_preview_presentation(host, &draft);
}

bool ide_workspace_authoring_host_cycle_presentation_tool(IDEWorkspaceAuthoringHost *host,
                                                          int delta) {
    IDEWorkspaceAuthoringProjection draft;
    int next;
    if (!host || !host->active || !host->presentation_draft_ready || delta == 0) return false;
    draft = host->draft_presentation;
    next = (int)draft.active_tool + delta;
    while (next < ICON_PROJECT_FILES) next += ICON_COUNT;
    while (next >= ICON_COUNT) next -= ICON_COUNT;
    draft.active_tool = (IconTool)next;
    return ide_workspace_authoring_session_adapter_preview_presentation(host, &draft);
}

bool ide_workspace_authoring_host_export_profile(IDEWorkspaceAuthoringHost *host) {
    char path[PATH_MAX];
    IDEWorkspaceAuthoringProfileResult result;
    if (!host || !host->active || !host->presentation_draft_ready ||
        !ide_workspace_authoring_profile_default_path(getWorkspacePath(), path, sizeof(path))) {
        authoring_set_status(host, "WAPP export needs an active workspace.");
        return false;
    }
    result = ide_workspace_authoring_profile_export_file(path, &host->draft_presentation);
    if (result != IDE_WORKSPACE_AUTHORING_PROFILE_OK) {
        authoring_set_status(host, ide_workspace_authoring_profile_result_string(result));
        requestFullRedraw(RENDER_INVALIDATION_OVERLAY);
        return false;
    }
    authoring_set_status(host, "WAPP saved: ide_files/workspace_authoring.wapp");
    requestFullRedraw(RENDER_INVALIDATION_OVERLAY);
    return true;
}

bool ide_workspace_authoring_host_preview_profile(IDEWorkspaceAuthoringHost *host) {
    char path[PATH_MAX];
    IDEWorkspaceAuthoringProfileResult result;
    if (!host || !host->active ||
        !ide_workspace_authoring_profile_default_path(getWorkspacePath(), path, sizeof(path))) {
        authoring_set_status(host, "WAPP preview needs an active workspace.");
        return false;
    }
    result = ide_workspace_authoring_session_adapter_preview_profile_file(host, path);
    if (result != IDE_WORKSPACE_AUTHORING_PROFILE_OK) {
        authoring_set_status(host, ide_workspace_authoring_profile_result_string(result));
        requestFullRedraw(RENDER_INVALIDATION_OVERLAY);
        return false;
    }
    authoring_set_status(host, "WAPP preview loaded; Apply commits, Esc restores.");
    requestFullRedraw(RENDER_INVALIDATION_OVERLAY);
    return true;
}

static bool authoring_apply_font_theme_action(IDEWorkspaceAuthoringHost *host,
                                              KitWorkspaceAuthoringFontThemeButtonId button_id) {
    KitWorkspaceAuthoringFontThemeAction action;
    const char *preset_name;
    char status[160];
    bool changed = false;

    if (!host || !host->active ||
        button_id == KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_NONE ||
        !kit_workspace_authoring_ui_font_theme_button_enabled(button_id)) {
        return false;
    }

    action = kit_workspace_authoring_ui_font_theme_action_for_button(button_id);
    switch (action.type) {
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_TEXT_SIZE_DEC:
            changed = ide_shared_font_step_by(-1);
            snprintf(status, sizeof(status), "Text size step: %d.", ide_shared_font_zoom_step());
            authoring_set_status(host, status);
            break;
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_TEXT_SIZE_INC:
            changed = ide_shared_font_step_by(1);
            snprintf(status, sizeof(status), "Text size step: %d.", ide_shared_font_zoom_step());
            authoring_set_status(host, status);
            break;
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_TEXT_SIZE_RESET:
            changed = ide_shared_font_reset_zoom_step();
            authoring_set_status(host, "Text size reset.");
            break;
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_SET_FONT_PRESET:
            preset_name = core_font_preset_name(action.font_preset_id);
            if (preset_name && ide_shared_font_set_preset(preset_name)) {
                changed = true;
                snprintf(status, sizeof(status), "Font preset: %s.", preset_name);
                authoring_set_status(host, status);
            }
            break;
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_SET_THEME_PRESET:
            preset_name = core_theme_preset_name(action.theme_preset_id);
            if (preset_name && ide_shared_theme_set_preset(preset_name)) {
                changed = true;
                snprintf(status, sizeof(status), "Theme preset: %s.", preset_name);
                authoring_set_status(host, status);
            }
            break;
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_CUSTOM_THEME_STATUS:
            authoring_set_status(host,
                                 action.custom_status_text ? action.custom_status_text
                                                           : "Custom theme action pending.");
            break;
        case KIT_WORKSPACE_AUTHORING_FONT_THEME_ACTION_NONE:
        default:
            return false;
    }

    host->font_theme_action_count += 1u;
    if (changed) {
        authoring_refresh_after_font_theme_change(RENDER_INVALIDATION_THEME |
                                                  RENDER_INVALIDATION_LAYOUT |
                                                  RENDER_INVALIDATION_RESIZE |
                                                  RENDER_INVALIDATION_CONTENT |
                                                  RENDER_INVALIDATION_BACKGROUND);
    } else {
        requestFullRedraw(RENDER_INVALIDATION_OVERLAY);
    }
    return true;
}

static bool authoring_handle_overlay_click(IDEWorkspaceAuthoringHost *host, int x, int y) {
    KitWorkspaceAuthoringOverlayButton buttons[4];
    uint32_t count = 0u;
    KitWorkspaceAuthoringOverlayButtonId hit = KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_NONE;

    if (!host || !host->active || host->viewport_width == 0u) return false;
    count = kit_workspace_authoring_ui_build_overlay_buttons(
        (int)host->viewport_width,
        1,
        ide_workspace_authoring_host_pane_overlay_active(host) ? 1 : 0,
        buttons,
        (uint32_t)(sizeof(buttons) / sizeof(buttons[0])));
    if (count > 3u) count = 3u;
    hit = kit_workspace_authoring_ui_overlay_hit_test(buttons, count, (float)x, (float)y);
    return ide_workspace_authoring_host_apply_overlay_button(host, hit);
}

static bool authoring_handle_presentation_click(IDEWorkspaceAuthoringHost *host, int x, int y) {
    IDEWorkspaceAuthoringPresentationControl controls[
        IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_COUNT];
    IDEWorkspaceAuthoringPresentationControlId hit;
    uint32_t count;

    if (!host || !ide_workspace_authoring_host_pane_overlay_active(host)) return false;
    count = ide_workspace_authoring_presentation_build_controls((int)host->viewport_width,
                                                                  &host->draft_presentation,
                                                                  controls,
                                                                  IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_COUNT);
    hit = ide_workspace_authoring_presentation_hit_test(controls, count, x, y);
    switch (hit) {
        case IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_TOOL_PANEL:
            return ide_workspace_authoring_host_toggle_presentation_field(host, 1u);
        case IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_CONTROL_PANEL:
            return ide_workspace_authoring_host_toggle_presentation_field(host, 2u);
        case IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_TERMINAL:
            return ide_workspace_authoring_host_toggle_presentation_field(host, 3u);
        case IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_TOOL_PREVIOUS:
            return ide_workspace_authoring_host_cycle_presentation_tool(host, -1);
        case IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_TOOL_NEXT:
            return ide_workspace_authoring_host_cycle_presentation_tool(host, 1);
        case IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_EXPORT_PROFILE:
            return ide_workspace_authoring_host_export_profile(host);
        case IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_PREVIEW_PROFILE:
            return ide_workspace_authoring_host_preview_profile(host);
        case IDE_WORKSPACE_AUTHORING_PRESENTATION_CONTROL_NONE:
        default:
            return false;
    }
}

static bool authoring_handle_font_theme_click(IDEWorkspaceAuthoringHost *host, int x, int y) {
    KitRenderContext kit_ctx;
    KitWorkspaceAuthoringFontThemeLayout layout;
    KitWorkspaceAuthoringFontThemeButtonId button_id;
    CoreResult result;

    if (!host || !host->active || host->viewport_width == 0u || host->viewport_height == 0u ||
        host->overlay_mode != IDE_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME) {
        return false;
    }

    memset(&kit_ctx, 0, sizeof(kit_ctx));
    result = kit_render_context_init(&kit_ctx,
                                     KIT_RENDER_BACKEND_NULL,
                                     CORE_THEME_PRESET_IDE_GRAY,
                                     CORE_FONT_PRESET_IDE);
    if (result.code == CORE_OK) {
        (void)kit_render_set_text_zoom_step(&kit_ctx, ide_shared_font_zoom_step());
    }

    if (!kit_workspace_authoring_ui_font_theme_build_layout(result.code == CORE_OK ? &kit_ctx : NULL,
                                                           (int)host->viewport_width,
                                                           (int)host->viewport_height,
                                                           &layout)) {
        if (result.code == CORE_OK) kit_render_context_shutdown(&kit_ctx);
        return false;
    }

    button_id = kit_workspace_authoring_ui_font_theme_hit_button(&layout, (float)x, (float)y);
    if (result.code == CORE_OK) kit_render_context_shutdown(&kit_ctx);
    return authoring_apply_font_theme_action(host, button_id);
}

bool ide_workspace_authoring_host_handle_sdl_event(IDEWorkspaceAuthoringHost *host,
                                                   const SDL_Event *event,
                                                   bool text_entry_active) {
    KitWorkspaceAuthoringKey key = KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
    uint32_t mod_bits = 0u;
    bool authoring_alt_only = false;
    int chord_pair_pressed = 0;

    if (!host || !event) return false;
    authoring_clear_event_flags(host);

    if (event->type == SDL_KEYUP) {
        key = authoring_key_from_sdl_keysym(&event->key.keysym);
        if (key == KIT_WORKSPACE_AUTHORING_KEY_C) host->key_c_down = 0u;
        if (key == KIT_WORKSPACE_AUTHORING_KEY_V) host->key_v_down = 0u;
        return false;
    }

    if (host->active &&
        event->type == SDL_MOUSEBUTTONDOWN &&
        event->button.button == SDL_BUTTON_LEFT &&
        authoring_handle_presentation_click(host, event->button.x, event->button.y)) {
        authoring_note_consumed(host, true);
        return true;
    }

    if (host->active &&
        event->type == SDL_MOUSEBUTTONDOWN &&
        event->button.button == SDL_BUTTON_LEFT &&
        authoring_handle_overlay_click(host, event->button.x, event->button.y)) {
        authoring_note_consumed(host, false);
        return true;
    }

    if (host->active &&
        event->type == SDL_MOUSEBUTTONDOWN &&
        event->button.button == SDL_BUTTON_LEFT &&
        authoring_handle_font_theme_click(host, event->button.x, event->button.y)) {
        authoring_note_consumed(host, false);
        return true;
    }

    if (host->active &&
        (event->type == SDL_MOUSEMOTION ||
         event->type == SDL_MOUSEBUTTONDOWN ||
         event->type == SDL_MOUSEBUTTONUP ||
         event->type == SDL_MOUSEWHEEL ||
         event->type == SDL_TEXTINPUT)) {
        authoring_note_consumed(host, true);
        return true;
    }

    if (event->type != SDL_KEYDOWN || event->key.repeat != 0) return false;

    key = authoring_key_from_sdl_keysym(&event->key.keysym);
    mod_bits = authoring_mod_bits((SDL_Keymod)event->key.keysym.mod);
    authoring_alt_only = ((mod_bits & KIT_WORKSPACE_AUTHORING_MOD_ALT) != 0u) &&
                         ((mod_bits & (KIT_WORKSPACE_AUTHORING_MOD_SHIFT |
                                       KIT_WORKSPACE_AUTHORING_MOD_CTRL |
                                       KIT_WORKSPACE_AUTHORING_MOD_GUI)) == 0u);

    if (text_entry_active && !host->active && !authoring_alt_only) return false;

    if (authoring_alt_only) {
        if (key == KIT_WORKSPACE_AUTHORING_KEY_C) host->key_c_down = 1u;
        if (key == KIT_WORKSPACE_AUTHORING_KEY_V) host->key_v_down = 1u;
    }

    chord_pair_pressed = kit_workspace_authoring_entry_chord_pressed(
        key,
        mod_bits,
        host->key_c_down ? 1 : 0,
        host->key_v_down ? 1 : 0);
    if (authoring_alt_only &&
        (key == KIT_WORKSPACE_AUTHORING_KEY_C || key == KIT_WORKSPACE_AUTHORING_KEY_V) &&
        host->entry_chord_armed_key != KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN &&
        host->entry_chord_armed_key != (uint8_t)key) {
        chord_pair_pressed = 1;
    }

    if (chord_pair_pressed) {
        if (host->active) {
            (void)ide_workspace_authoring_session_adapter_cancel(host);
        } else {
            (void)ide_workspace_authoring_session_adapter_enter(host);
        }
        host->entry_chord_armed_key = KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
        authoring_note_consumed(host, false);
        return true;
    }

    if (authoring_alt_only &&
        (key == KIT_WORKSPACE_AUTHORING_KEY_C || key == KIT_WORKSPACE_AUTHORING_KEY_V)) {
        host->entry_chord_armed_key = (uint8_t)key;
        authoring_note_consumed(host, false);
        return true;
    }

    if (!host->active) return false;

    if (key == KIT_WORKSPACE_AUTHORING_KEY_TAB) {
        authoring_cycle_overlay(host);
        authoring_note_consumed(host, true);
        return true;
    }
    if (host->overlay_mode == IDE_WORKSPACE_AUTHORING_OVERLAY_PANES) {
        if (event->key.keysym.sym == SDLK_1 || event->key.keysym.sym == SDLK_2 ||
            event->key.keysym.sym == SDLK_3) {
            (void)ide_workspace_authoring_host_toggle_presentation_field(
                host, (unsigned int)(event->key.keysym.sym - SDLK_0));
            authoring_note_consumed(host, true);
            return true;
        }
        if (event->key.keysym.sym == SDLK_LEFT || event->key.keysym.sym == SDLK_RIGHT) {
            (void)ide_workspace_authoring_host_cycle_presentation_tool(
                host, event->key.keysym.sym == SDLK_LEFT ? -1 : 1);
            authoring_note_consumed(host, true);
            return true;
        }
    }
    if (key == KIT_WORKSPACE_AUTHORING_KEY_ENTER) {
        (void)ide_workspace_authoring_session_adapter_apply(host);
        authoring_note_consumed(host, true);
        return true;
    }
    if (key == KIT_WORKSPACE_AUTHORING_KEY_ESCAPE) {
        (void)ide_workspace_authoring_session_adapter_cancel(host);
        authoring_note_consumed(host, true);
        return true;
    }

    authoring_note_consumed(host, true);
    return true;
}
