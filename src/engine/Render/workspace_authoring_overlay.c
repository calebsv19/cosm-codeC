#include "engine/Render/workspace_authoring_overlay.h"

#include <stdio.h>
#include <string.h>

#include <SDL2/SDL.h>

#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/workspace_authoring_host.h"
#include "engine/Render/render_font.h"
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_pipeline.h"
#include "ide/Panes/PaneInfo/pane.h"
#include "ide/UI/panel_control_widgets.h"
#include "ide/UI/shared_theme_font_adapter.h"

static SDL_Rect authoring_sdl_kit_rect(KitRenderRect rect) {
    SDL_Rect out;
    out.x = (int)rect.x;
    out.y = (int)rect.y;
    out.w = (int)rect.width;
    out.h = (int)rect.height;
    if (out.w < 0) out.w = 0;
    if (out.h < 0) out.h = 0;
    return out;
}

static SDL_Rect authoring_sdl_rect(CorePaneRect rect) {
    SDL_Rect out;
    out.x = (int)rect.x;
    out.y = (int)rect.y;
    out.w = (int)rect.width;
    out.h = (int)rect.height;
    if (out.w < 0) out.w = 0;
    if (out.h < 0) out.h = 0;
    return out;
}

static const char *authoring_pane_role_label(UIPaneRole role) {
    switch (role) {
        case PANE_ROLE_MENUBAR: return "Menu Bar";
        case PANE_ROLE_EDITOR: return "Editor";
        case PANE_ROLE_ICONBAR: return "Icon Bar";
        case PANE_ROLE_TOOLPANEL: return "Tool Panel";
        case PANE_ROLE_CONTROLPANEL: return "Control Panel";
        case PANE_ROLE_TERMINAL: return "Terminal";
        case PANE_ROLE_POPUP: return "Popup";
        case PANE_ROLE_UNKNOWN:
        default: return "Unknown";
    }
}

static void authoring_draw_text_tier(SDL_Renderer *renderer,
                                     int x,
                                     int y,
                                     SDL_Color color,
                                     CoreFontTextSizeTier tier,
                                     const char *text) {
    TTF_Font *font = getUIFontByTier(tier);
    (void)renderer;
    if (!font) font = getActiveFont();
    drawTextUTF8WithFontColor(x, y, text ? text : "", font, color, false);
}

static void authoring_draw_text(SDL_Renderer *renderer,
                                int x,
                                int y,
                                SDL_Color color,
                                const char *text) {
    TTF_Font *font = getUIFontByTier(CORE_FONT_TEXT_SIZE_CAPTION);
    (void)renderer;
    if (!font) font = getActiveFont();
    drawTextUTF8WithFontColor(x, y, text ? text : "", font, color, false);
}

static void authoring_draw_section(SDL_Renderer *renderer,
                                   const IDEThemePalette *palette,
                                   KitRenderRect kit_rect,
                                   const char *title,
                                   const char *detail) {
    SDL_Rect rect;
    if (!renderer || !palette) return;
    rect = authoring_sdl_kit_rect(kit_rect);
    SDL_SetRenderDrawColor(renderer,
                           palette->pane_body_fill.r,
                           palette->pane_body_fill.g,
                           palette->pane_body_fill.b,
                           245);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer,
                           palette->pane_border.r,
                           palette->pane_border.g,
                           palette->pane_border.b,
                           210);
    SDL_RenderDrawRect(renderer, &rect);
    authoring_draw_text_tier(renderer,
                             rect.x + 12,
                             rect.y + 10,
                             palette->text_primary,
                             CORE_FONT_TEXT_SIZE_BASIC,
                             title);
    if (detail && detail[0]) {
        authoring_draw_text(renderer, rect.x + 12, rect.y + 34, palette->text_muted, detail);
    }
}

static void authoring_draw_font_theme_button(SDL_Renderer *renderer,
                                             const IDEThemePalette *palette,
                                             KitRenderRect kit_rect,
                                             const char *label,
                                             bool enabled,
                                             bool active,
                                             int mouse_x,
                                             int mouse_y,
                                             bool mouse_pressed) {
    SDL_Rect rect;
    bool hovered;

    if (!renderer || !palette) return;
    rect = authoring_sdl_kit_rect(kit_rect);
    hovered = ui_panel_rect_contains(&rect, mouse_x, mouse_y);
    ui_panel_compact_button_render(renderer,
                                   &(UIPanelCompactButtonSpec){
                                       .rect = rect,
                                       .label = label,
                                       .hovered = hovered,
                                       .active = active,
                                       .pressed = hovered && mouse_pressed && enabled,
                                       .disabled = !enabled,
                                       .outlined = false,
                                       .use_custom_fill = false,
                                       .use_custom_outline = false,
                                       .tier = CORE_FONT_TEXT_SIZE_CAPTION
                                   });
}

static int authoring_zoom_percent(void) {
    int pct = 100 + (ide_shared_font_zoom_step() * 10);
    if (pct < 60) pct = 60;
    if (pct > 180) pct = 180;
    return pct;
}

static void authoring_draw_button(SDL_Renderer *renderer,
                                  const IDEThemePalette *palette,
                                  const KitWorkspaceAuthoringOverlayButton *button,
                                  int mouse_x,
                                  int mouse_y,
                                  bool mouse_pressed) {
    SDL_Rect rect;
    bool active;
    bool enabled;
    bool hovered;

    if (!renderer || !palette || !button || !button->visible) return;

    rect = authoring_sdl_rect(button->rect);
    enabled = button->enabled != 0u;
    active = button->id == KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_MODE;
    hovered = ui_panel_rect_contains(&rect, mouse_x, mouse_y);
    ui_panel_compact_button_render(renderer,
                                   &(UIPanelCompactButtonSpec){
                                       .rect = rect,
                                       .label = button->label,
                                       .hovered = hovered,
                                       .active = active,
                                       .pressed = hovered && mouse_pressed && enabled,
                                       .disabled = !enabled,
                                       .outlined = false,
                                       .use_custom_fill = false,
                                       .use_custom_outline = false,
                                       .tier = CORE_FONT_TEXT_SIZE_CAPTION
                                   });
}

static void authoring_draw_controls(SDL_Renderer *renderer,
                                    IDEWorkspaceAuthoringHost *host,
                                    const IDEThemePalette *palette,
                                    int viewport_width) {
    KitWorkspaceAuthoringOverlayButton buttons[4];
    uint32_t count;
    uint32_t i;
    int mouse_x = 0;
    int mouse_y = 0;
    Uint32 mouse_buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
    bool mouse_pressed = (mouse_buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;

    count = kit_workspace_authoring_ui_build_overlay_buttons(
        viewport_width,
        ide_workspace_authoring_host_active(host) ? 1 : 0,
        ide_workspace_authoring_host_pane_overlay_active(host) ? 1 : 0,
        buttons,
        (uint32_t)(sizeof(buttons) / sizeof(buttons[0])));
    for (i = 0u; i < count; ++i) {
        authoring_draw_button(renderer, palette, &buttons[i], mouse_x, mouse_y, mouse_pressed);
    }
}

static void authoring_draw_pane_readout(SDL_Renderer *renderer,
                                        IDEWorkspaceAuthoringHost *host,
                                        const IDEThemePalette *palette,
                                        UIPane **panes,
                                        int pane_count) {
    int i;
    char label[256];

    if (!renderer || !host || !palette || !panes) return;

    for (i = 0; i < pane_count; ++i) {
        UIPane *pane = panes[i];
        SDL_Rect rect;
        if (!pane || !pane->visible || pane->w <= 0 || pane->h <= 0) continue;

        rect.x = pane->x;
        rect.y = pane->y;
        rect.w = pane->w;
        rect.h = pane->h;
        SDL_SetRenderDrawColor(renderer,
                               palette->accent_primary.r,
                               palette->accent_primary.g,
                               palette->accent_primary.b,
                               95);
        SDL_RenderDrawRect(renderer, &rect);

        snprintf(label,
                 sizeof(label),
                 "P%d %s%s%s",
                 i + 1,
                 authoring_pane_role_label(pane->role),
                 pane->title && pane->title[0] ? ": " : "",
                 pane->title && pane->title[0] ? pane->title : "");
        SDL_Rect tag = {pane->x + 6, pane->y + 6, 220, 22};
        if (tag.w > pane->w - 12) tag.w = pane->w - 12;
        if (tag.w < 80) tag.w = 80;
        SDL_SetRenderDrawColor(renderer,
                               palette->pane_header_fill.r,
                               palette->pane_header_fill.g,
                               palette->pane_header_fill.b,
                               220);
        SDL_RenderFillRect(renderer, &tag);
        SDL_SetRenderDrawColor(renderer,
                               palette->pane_border.r,
                               palette->pane_border.g,
                               palette->pane_border.b,
                               220);
        SDL_RenderDrawRect(renderer, &tag);
        authoring_draw_text(renderer, tag.x + 6, tag.y + 3, palette->text_primary, label);
    }

    if (host->status_text[0]) {
        authoring_draw_text(renderer, 16, 52, palette->text_muted, host->status_text);
    } else {
        authoring_draw_text(renderer, 16, 52, palette->text_muted,
                            "Tab switches authoring overlay. Enter applies. Esc cancels.");
    }
}

static bool authoring_build_font_theme_layout(int viewport_width,
                                              int viewport_height,
                                              KitWorkspaceAuthoringFontThemeLayout *out_layout) {
    KitRenderContext kit_ctx;
    CoreResult result;

    if (!out_layout) return false;
    memset(&kit_ctx, 0, sizeof(kit_ctx));
    result = kit_render_context_init(&kit_ctx,
                                     KIT_RENDER_BACKEND_NULL,
                                     CORE_THEME_PRESET_IDE_GRAY,
                                     CORE_FONT_PRESET_IDE);
    if (result.code == CORE_OK) {
        (void)kit_render_set_text_zoom_step(&kit_ctx, ide_shared_font_zoom_step());
    }
    if (!kit_workspace_authoring_ui_font_theme_build_layout(result.code == CORE_OK ? &kit_ctx : NULL,
                                                           viewport_width,
                                                           viewport_height,
                                                           out_layout)) {
        if (result.code == CORE_OK) kit_render_context_shutdown(&kit_ctx);
        return false;
    }
    if (result.code == CORE_OK) kit_render_context_shutdown(&kit_ctx);
    return true;
}

static void authoring_draw_font_theme_overlay(SDL_Renderer *renderer,
                                              IDEWorkspaceAuthoringHost *host,
                                              const IDEThemePalette *palette,
                                              int viewport_width,
                                              int viewport_height) {
    KitWorkspaceAuthoringFontThemeLayout layout;
    SDL_Rect screen;
    SDL_Rect panel;
    char font_detail[160];
    char size_detail[160];
    char theme_detail[160];
    char custom_detail[160];
    char current_font[64] = "ide";
    char current_theme[64] = "studio_blue";
    char chip_label[48];
    uint32_t i;
    int mouse_x = 0;
    int mouse_y = 0;
    Uint32 mouse_buttons = SDL_GetMouseState(&mouse_x, &mouse_y);
    bool mouse_pressed = (mouse_buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;

    if (!renderer || !host || !palette) return;
    SDL_SetRenderDrawColor(renderer,
                           palette->app_background.r,
                           palette->app_background.g,
                           palette->app_background.b,
                           238);
    screen = (SDL_Rect){0, 0, viewport_width, viewport_height};
    SDL_RenderFillRect(renderer, &screen);

    if (!authoring_build_font_theme_layout(viewport_width, viewport_height, &layout)) {
        authoring_draw_text(renderer, 24, 72, palette->text_primary, "Font/Theme layout unavailable.");
        return;
    }

    (void)ide_shared_font_current_preset(current_font, sizeof(current_font));
    (void)ide_shared_theme_current_preset(current_theme, sizeof(current_theme));
    snprintf(font_detail, sizeof(font_detail), "Current font preset: %s", current_font);
    snprintf(size_detail,
             sizeof(size_detail),
             "Text Size step:%+d (%d%%)",
             ide_shared_font_zoom_step(),
             authoring_zoom_percent());
    snprintf(theme_detail, sizeof(theme_detail), "Current theme preset: %s", current_theme);
    snprintf(custom_detail,
             sizeof(custom_detail),
             "%s",
             "Custom theme slots stay stubbed until the theme editor lane is promoted.");
    snprintf(chip_label,
             sizeof(chip_label),
             "%+d",
             ide_shared_font_zoom_step());

    panel = authoring_sdl_kit_rect(layout.panel);
    SDL_SetRenderDrawColor(renderer,
                           palette->modal_fill.r,
                           palette->modal_fill.g,
                           palette->modal_fill.b,
                           245);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer,
                           palette->modal_border.r,
                           palette->modal_border.g,
                           palette->modal_border.b,
                           245);
    SDL_RenderDrawRect(renderer, &panel);

    authoring_draw_text_tier(renderer,
                             panel.x + 12,
                             panel.y + 10,
                             palette->text_primary,
                             CORE_FONT_TEXT_SIZE_BASIC,
                             "Font/Theme Overlay");

    authoring_draw_section(renderer, palette, layout.font_preset_section, "Font Preset", font_detail);
    authoring_draw_section(renderer, palette, layout.text_size_section, "Text Size", size_detail);
    authoring_draw_section(renderer, palette, layout.theme_preset_section, "Theme Preset", theme_detail);
    authoring_draw_section(renderer, palette, layout.custom_theme_section, "Custom Presets", custom_detail);

    for (i = 0u; i < layout.font_preset_button_count &&
                i < KIT_WORKSPACE_AUTHORING_FONT_THEME_FONT_PRESET_BUTTON_COUNT; ++i) {
        KitWorkspaceAuthoringFontThemeButtonId button_id =
            (KitWorkspaceAuthoringFontThemeButtonId)(
                KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_FONT_PRESET_DAW_DEFAULT + i);
        const char *label = kit_workspace_authoring_ui_font_theme_button_label(button_id);
        authoring_draw_font_theme_button(
            renderer,
            palette,
            layout.font_preset_buttons[i],
            label,
            kit_workspace_authoring_ui_font_theme_button_enabled(button_id) != 0u,
            strcmp(label, current_font) == 0,
            mouse_x,
            mouse_y,
            mouse_pressed);
    }

    authoring_draw_font_theme_button(renderer,
                                     palette,
                                     layout.text_size_dec_button,
                                     kit_workspace_authoring_ui_font_theme_button_label(
                                         KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_DEC),
                                     true,
                                     false,
                                     mouse_x,
                                     mouse_y,
                                     mouse_pressed);
    authoring_draw_font_theme_button(renderer,
                                     palette,
                                     layout.text_size_inc_button,
                                     kit_workspace_authoring_ui_font_theme_button_label(
                                         KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_INC),
                                     true,
                                     false,
                                     mouse_x,
                                     mouse_y,
                                     mouse_pressed);
    authoring_draw_font_theme_button(renderer,
                                     palette,
                                     layout.text_size_value_chip,
                                     chip_label,
                                     false,
                                     true,
                                     mouse_x,
                                     mouse_y,
                                     mouse_pressed);
    authoring_draw_font_theme_button(renderer,
                                     palette,
                                     layout.text_size_reset_button,
                                     kit_workspace_authoring_ui_font_theme_button_label(
                                         KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_TEXT_SIZE_RESET),
                                     true,
                                     ide_shared_font_zoom_step() == 0,
                                     mouse_x,
                                     mouse_y,
                                     mouse_pressed);

    for (i = 0u; i < layout.theme_preset_button_count &&
                i < KIT_WORKSPACE_AUTHORING_FONT_THEME_THEME_PRESET_BUTTON_COUNT; ++i) {
        KitWorkspaceAuthoringFontThemeButtonId button_id =
            (KitWorkspaceAuthoringFontThemeButtonId)(
                KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_THEME_PRESET_DAW_DEFAULT + i);
        const char *label = kit_workspace_authoring_ui_font_theme_button_label(button_id);
        authoring_draw_font_theme_button(
            renderer,
            palette,
            layout.theme_preset_buttons[i],
            label,
            kit_workspace_authoring_ui_font_theme_button_enabled(button_id) != 0u,
            strcmp(label, current_theme) == 0,
            mouse_x,
            mouse_y,
            mouse_pressed);
    }

    for (i = 0u; i < layout.custom_theme_button_count &&
                i < KIT_WORKSPACE_AUTHORING_FONT_THEME_CUSTOM_THEME_BUTTON_COUNT; ++i) {
        KitWorkspaceAuthoringFontThemeButtonId button_id =
            (KitWorkspaceAuthoringFontThemeButtonId)(
                KIT_WORKSPACE_AUTHORING_FONT_THEME_BUTTON_CUSTOM_THEME_CREATE_STUB + i);
        authoring_draw_font_theme_button(renderer,
                                         palette,
                                         layout.custom_theme_buttons[i],
                                         kit_workspace_authoring_ui_font_theme_button_label(button_id),
                                         true,
                                         false,
                                         mouse_x,
                                         mouse_y,
                                         mouse_pressed);
    }

    if (host->status_text[0]) {
        authoring_draw_text(renderer,
                            panel.x + 12,
                            panel.y + panel.h - 22,
                            palette->text_muted,
                            host->status_text);
    }
}

void ide_workspace_authoring_overlay_render(IDECoreState *core,
                                            UIPane **panes,
                                            int pane_count,
                                            int viewport_width,
                                            int viewport_height) {
    RenderContext *ctx = getRenderContext();
    SDL_Renderer *renderer;
    IDEThemePalette palette;

    if (!core || !ide_workspace_authoring_host_active(&core->workspaceAuthoring) ||
        !ctx || !ctx->renderer) {
        return;
    }
    if (!ide_shared_theme_resolve_palette(&palette)) {
        return;
    }

    ide_workspace_authoring_host_set_viewport(&core->workspaceAuthoring,
                                              (uint32_t)viewport_width,
                                              (uint32_t)viewport_height);
    renderer = (SDL_Renderer *)ctx->renderer;
    authoring_draw_controls(renderer, &core->workspaceAuthoring, &palette, viewport_width);
    if (ide_workspace_authoring_host_pane_overlay_active(&core->workspaceAuthoring)) {
        authoring_draw_pane_readout(renderer,
                                    &core->workspaceAuthoring,
                                    &palette,
                                    panes,
                                    pane_count);
    } else {
        authoring_draw_font_theme_overlay(renderer,
                                          &core->workspaceAuthoring,
                                          &palette,
                                          viewport_width,
                                          viewport_height);
    }
}
