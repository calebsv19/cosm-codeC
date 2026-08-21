#ifndef IDE_WORKSPACE_AUTHORING_HOST_H
#define IDE_WORKSPACE_AUTHORING_HOST_H

#include <stdbool.h>
#include <stdint.h>

#include <SDL2/SDL.h>

#include "kit_workspace_authoring.h"
#include "kit_workspace_authoring_ui.h"
#include "core_workspace_authoring_session.h"
#include "app/GlobalInfo/workspace_authoring_projection.h"

typedef enum IDEWorkspaceAuthoringOverlayMode {
    IDE_WORKSPACE_AUTHORING_OVERLAY_PANES = 0,
    IDE_WORKSPACE_AUTHORING_OVERLAY_FONT_THEME
} IDEWorkspaceAuthoringOverlayMode;

typedef struct IDEWorkspaceAuthoringHost {
    CoreWorkspaceAuthoringSession session;
    uint8_t active;
    uint8_t key_c_down;
    uint8_t key_v_down;
    uint8_t entry_chord_armed_key;
    uint8_t last_event_consumed;
    uint8_t last_event_entered;
    uint8_t last_event_exited;
    uint8_t last_event_accepted;
    uint8_t last_event_canceled;
    uint8_t overlay_mode;
    uint32_t viewport_width;
    uint32_t viewport_height;
    uint32_t enter_count;
    uint32_t apply_count;
    uint32_t cancel_count;
    uint32_t overlay_cycle_count;
    uint32_t consumed_event_count;
    uint32_t captured_runtime_event_count;
    uint32_t add_stub_count;
    uint32_t font_theme_action_count;
    int baseline_font_zoom_step;
    IDEWorkspaceAuthoringProjection baseline_presentation;
    IDEWorkspaceAuthoringProjection draft_presentation;
    uint8_t presentation_draft_ready;
    char baseline_theme_preset[64];
    char baseline_font_preset[64];
    char status_text[160];
} IDEWorkspaceAuthoringHost;

void ide_workspace_authoring_host_reset(IDEWorkspaceAuthoringHost *host);
void ide_workspace_authoring_host_set_viewport(IDEWorkspaceAuthoringHost *host,
                                               uint32_t width,
                                               uint32_t height);
bool ide_workspace_authoring_host_active(const IDEWorkspaceAuthoringHost *host);
bool ide_workspace_authoring_host_pane_overlay_active(const IDEWorkspaceAuthoringHost *host);
bool ide_workspace_authoring_host_font_theme_overlay_active(const IDEWorkspaceAuthoringHost *host);
bool ide_workspace_authoring_host_handle_sdl_event(IDEWorkspaceAuthoringHost *host,
                                                   const SDL_Event *event,
                                                   bool text_entry_active);
bool ide_workspace_authoring_host_apply_overlay_button(
    IDEWorkspaceAuthoringHost *host,
    KitWorkspaceAuthoringOverlayButtonId button_id);
bool ide_workspace_authoring_host_toggle_presentation_field(IDEWorkspaceAuthoringHost *host,
                                                            unsigned int field_index);
bool ide_workspace_authoring_host_cycle_presentation_tool(IDEWorkspaceAuthoringHost *host,
                                                          int delta);
bool ide_workspace_authoring_host_export_profile(IDEWorkspaceAuthoringHost *host);
bool ide_workspace_authoring_host_preview_profile(IDEWorkspaceAuthoringHost *host);

#endif
