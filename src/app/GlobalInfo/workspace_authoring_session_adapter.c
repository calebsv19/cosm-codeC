#include "app/GlobalInfo/workspace_authoring_session_adapter.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "app/GlobalInfo/workspace_prefs.h"
#include "app/GlobalInfo/workspace_authoring_projection.h"
#include "engine/Render/render_font.h"
#include "engine/Render/render_helpers.h"
#include "ide/Panes/Terminal/terminal.h"
#include "ide/UI/layout.h"
#include "ide/UI/shared_theme_font_adapter.h"

static void adapter_set_status(IDEWorkspaceAuthoringHost *host, const char *status) {
    if (!host) return;
    if (!status) {
        host->status_text[0] = '\0';
        return;
    }
    strncpy(host->status_text, status, sizeof(host->status_text) - 1u);
    host->status_text[sizeof(host->status_text) - 1u] = '\0';
}

static void adapter_refresh_after_font_theme_change(uint32_t reason_bits) {
    render_text_cache_shutdown();
    (void)reloadFontSystem();
    terminal_notify_font_metrics_changed();
    ide_refresh_live_theme();
    requestFullRedraw(reason_bits);
}

static CoreWorkspaceAuthoringSessionHookResult adapter_begin_authoring(void *context) {
    IDEWorkspaceAuthoringHost *host = context;
    UIState *ui = getUIState();
    if (!host || host->active) return CORE_WORKSPACE_AUTHORING_SESSION_HOOK_REJECTED;
    if (!ide_workspace_authoring_projection_capture(&host->baseline_presentation,
                                                    ui,
                                                    getActiveIcon())) {
        return CORE_WORKSPACE_AUTHORING_SESSION_HOOK_FAILED;
    }
    host->draft_presentation = host->baseline_presentation;
    host->presentation_draft_ready = 1u;

    host->active = 1u;
    host->overlay_mode = IDE_WORKSPACE_AUTHORING_OVERLAY_PANES;
    host->enter_count += 1u;
    host->baseline_font_zoom_step = ide_shared_font_zoom_step();
    if (!ide_shared_theme_current_preset(host->baseline_theme_preset,
                                         sizeof(host->baseline_theme_preset))) {
        host->baseline_theme_preset[0] = '\0';
    }
    if (!ide_shared_font_current_preset(host->baseline_font_preset,
                                        sizeof(host->baseline_font_preset))) {
        host->baseline_font_preset[0] = '\0';
    }
    host->last_event_entered = 1u;
    adapter_set_status(host, "Authoring active.");
    return CORE_WORKSPACE_AUTHORING_SESSION_HOOK_OK;
}

static CoreWorkspaceAuthoringSessionHookResult adapter_validate_draft(void *context) {
    IDEWorkspaceAuthoringHost *host = context;
    return host && host->presentation_draft_ready &&
                   ide_workspace_authoring_projection_valid(&host->draft_presentation)
               ? CORE_WORKSPACE_AUTHORING_SESSION_HOOK_OK
               : CORE_WORKSPACE_AUTHORING_SESSION_HOOK_FAILED;
}

static CoreWorkspaceAuthoringSessionHookResult adapter_apply_draft(void *context) {
    IDEWorkspaceAuthoringHost *host = context;
    char theme_preset[64] = {0};
    char font_preset[64] = {0};
    char zoom_step_buf[16];
    int zoom_step;

    if (!host || !host->active) return CORE_WORKSPACE_AUTHORING_SESSION_HOOK_FAILED;
    if (!ide_workspace_authoring_projection_apply(&host->draft_presentation, getUIState())) {
        return CORE_WORKSPACE_AUTHORING_SESSION_HOOK_FAILED;
    }
    saveWorkspaceAuthoringPresentationPreference(host->draft_presentation.tool_panel_visible,
                                                host->draft_presentation.control_panel_visible,
                                                host->draft_presentation.terminal_visible,
                                                host->draft_presentation.active_tool);
    zoom_step = ide_shared_font_zoom_step();
    if (ide_shared_theme_current_preset(theme_preset, sizeof(theme_preset))) {
        saveThemePresetPreference(theme_preset);
    }
    if (ide_shared_font_current_preset(font_preset, sizeof(font_preset))) {
        saveFontPresetPreference(font_preset);
    }
    saveFontZoomStepPreference(zoom_step);
    snprintf(zoom_step_buf, sizeof(zoom_step_buf), "%d", zoom_step);
    setenv("IDE_FONT_ZOOM_STEP", zoom_step_buf, 1);
    host->apply_count += 1u;
    host->last_event_accepted = 1u;
    adapter_set_status(host, "Authoring applied.");
    return CORE_WORKSPACE_AUTHORING_SESSION_HOOK_OK;
}

static CoreWorkspaceAuthoringSessionHookResult adapter_cancel_draft(void *context) {
    IDEWorkspaceAuthoringHost *host = context;
    if (!host || !host->active) return CORE_WORKSPACE_AUTHORING_SESSION_HOOK_FAILED;
    if (host->presentation_draft_ready &&
        !ide_workspace_authoring_projection_apply(&host->baseline_presentation, getUIState())) {
        return CORE_WORKSPACE_AUTHORING_SESSION_HOOK_FAILED;
    }
    if (host->baseline_theme_preset[0]) {
        (void)ide_shared_theme_set_preset(host->baseline_theme_preset);
    }
    if (host->baseline_font_preset[0]) {
        (void)ide_shared_font_set_preset(host->baseline_font_preset);
    }
    (void)ide_shared_font_set_zoom_step(host->baseline_font_zoom_step);
    adapter_refresh_after_font_theme_change(RENDER_INVALIDATION_THEME |
                                            RENDER_INVALIDATION_LAYOUT |
                                            RENDER_INVALIDATION_RESIZE |
                                            RENDER_INVALIDATION_CONTENT |
                                            RENDER_INVALIDATION_BACKGROUND);
    host->cancel_count += 1u;
    host->last_event_canceled = 1u;
    adapter_set_status(host, "Authoring canceled.");
    return CORE_WORKSPACE_AUTHORING_SESSION_HOOK_OK;
}

static CoreWorkspaceAuthoringSessionHookResult adapter_resume_runtime(void *context) {
    IDEWorkspaceAuthoringHost *host = context;
    if (!host) return CORE_WORKSPACE_AUTHORING_SESSION_HOOK_FAILED;
    host->active = 0u;
    host->key_c_down = 0u;
    host->key_v_down = 0u;
    host->entry_chord_armed_key = KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
    host->overlay_mode = IDE_WORKSPACE_AUTHORING_OVERLAY_PANES;
    host->last_event_exited = 1u;
    host->presentation_draft_ready = 0u;
    return CORE_WORKSPACE_AUTHORING_SESSION_HOOK_OK;
}

static CoreWorkspaceAuthoringSessionHookResult adapter_recover_failed_safe(void *context) {
    IDEWorkspaceAuthoringHost *host = context;
    if (!host) return CORE_WORKSPACE_AUTHORING_SESSION_HOOK_FAILED;
    if (host->active) (void)adapter_cancel_draft(host);
    host->active = 0u;
    host->key_c_down = 0u;
    host->key_v_down = 0u;
    host->entry_chord_armed_key = KIT_WORKSPACE_AUTHORING_KEY_UNKNOWN;
    host->overlay_mode = IDE_WORKSPACE_AUTHORING_OVERLAY_PANES;
    host->last_event_exited = 1u;
    adapter_set_status(host, "Authoring recovered safely.");
    return CORE_WORKSPACE_AUTHORING_SESSION_HOOK_OK;
}

void ide_workspace_authoring_session_adapter_reset(IDEWorkspaceAuthoringHost *host) {
    static const CoreWorkspaceAuthoringSessionHooks hooks = {
        adapter_begin_authoring,
        adapter_validate_draft,
        adapter_apply_draft,
        adapter_cancel_draft,
        adapter_resume_runtime,
        adapter_recover_failed_safe
    };
    if (!host) return;
    core_workspace_authoring_session_init(
        &host->session,
        CORE_WORKSPACE_AUTHORING_CAP_SAFE_RUNTIME_GATE |
            CORE_WORKSPACE_AUTHORING_CAP_FONT_THEME_DRAFT,
        host,
        &hooks);
}

CoreWorkspaceAuthoringSessionOutcome ide_workspace_authoring_session_adapter_enter(
    IDEWorkspaceAuthoringHost *host) {
    return host ? core_workspace_authoring_session_enter(&host->session)
                : CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_REJECTED;
}

CoreWorkspaceAuthoringSessionOutcome ide_workspace_authoring_session_adapter_apply(
    IDEWorkspaceAuthoringHost *host) {
    return host ? core_workspace_authoring_session_apply(&host->session)
                : CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_REJECTED;
}

CoreWorkspaceAuthoringSessionOutcome ide_workspace_authoring_session_adapter_cancel(
    IDEWorkspaceAuthoringHost *host) {
    CoreWorkspaceAuthoringSessionOutcome outcome;
    if (!host) return CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_REJECTED;
    outcome = core_workspace_authoring_session_cancel(&host->session);
    if (outcome == CORE_WORKSPACE_AUTHORING_SESSION_OUTCOME_FAILED_SAFE) {
        return core_workspace_authoring_session_recover(&host->session);
    }
    return outcome;
}

bool ide_workspace_authoring_session_adapter_preview_presentation(
    IDEWorkspaceAuthoringHost *host,
    const IDEWorkspaceAuthoringProjection *draft) {
    if (!host || !draft || !host->active || !host->presentation_draft_ready ||
        !ide_workspace_authoring_projection_valid(draft) ||
        !ide_workspace_authoring_projection_apply(draft, getUIState())) {
        return false;
    }
    host->draft_presentation = *draft;
    requestFullRedraw(RENDER_INVALIDATION_LAYOUT | RENDER_INVALIDATION_CONTENT |
                      RENDER_INVALIDATION_OVERLAY);
    adapter_set_status(host, "Presentation draft preview active. Enter applies; Esc restores.");
    return true;
}

IDEWorkspaceAuthoringProfileResult ide_workspace_authoring_session_adapter_preview_profile_file(
    IDEWorkspaceAuthoringHost *host,
    const char *path) {
    IDEWorkspaceAuthoringProjection imported;
    IDEWorkspaceAuthoringProfileResult result;

    if (!host || !ide_workspace_authoring_host_active(host)) {
        return IDE_WORKSPACE_AUTHORING_PROFILE_ERR_INVALID_ARG;
    }
    result = ide_workspace_authoring_profile_import_file(path, &imported);
    if (result != IDE_WORKSPACE_AUTHORING_PROFILE_OK) {
        return result;
    }
    return ide_workspace_authoring_session_adapter_preview_presentation(host, &imported)
               ? IDE_WORKSPACE_AUTHORING_PROFILE_OK
               : IDE_WORKSPACE_AUTHORING_PROFILE_ERR_PROJECTION;
}
