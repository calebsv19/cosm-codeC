#include "engine/Render/workspace_authoring_overlay.h"

#include <stdio.h>

#include <SDL2/SDL.h>

#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/workspace_authoring_host.h"
#include "engine/Render/render_font.h"
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_pipeline.h"
#include "ide/Panes/PaneInfo/pane.h"
#include "ide/UI/shared_theme_font_adapter.h"

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

static SDL_Color authoring_alpha(SDL_Color color, Uint8 alpha) {
    color.a = alpha;
    return color;
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

static void authoring_draw_button(SDL_Renderer *renderer,
                                  const IDEThemePalette *palette,
                                  const KitWorkspaceAuthoringOverlayButton *button) {
    SDL_Rect rect;
    SDL_Color fill;
    SDL_Color border;
    SDL_Color text;

    if (!renderer || !palette || !button || !button->visible) return;

    rect = authoring_sdl_rect(button->rect);
    fill = authoring_alpha(palette->button_fill, button->enabled ? 238u : 130u);
    border = authoring_alpha(palette->button_border, button->enabled ? 245u : 140u);
    text = button->enabled ? palette->text_primary : authoring_alpha(palette->text_muted, 150u);

    if (button->id == KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_MODE) {
        fill = authoring_alpha(palette->accent_primary, 228u);
        text = palette->app_background;
    } else if (button->id == KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_APPLY) {
        fill = authoring_alpha((SDL_Color){132, 190, 142, 255}, 226u);
        text = palette->app_background;
    } else if (button->id == KIT_WORKSPACE_AUTHORING_OVERLAY_BUTTON_CANCEL) {
        fill = authoring_alpha(palette->accent_warning, 224u);
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, fill.a);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, border.a);
    SDL_RenderDrawRect(renderer, &rect);
    authoring_draw_text(renderer, rect.x + 7, rect.y + 3, text, button->label);
}

static void authoring_draw_controls(SDL_Renderer *renderer,
                                    IDEWorkspaceAuthoringHost *host,
                                    const IDEThemePalette *palette,
                                    int viewport_width) {
    KitWorkspaceAuthoringOverlayButton buttons[4];
    uint32_t count;
    uint32_t i;

    count = kit_workspace_authoring_ui_build_overlay_buttons(
        viewport_width,
        ide_workspace_authoring_host_active(host) ? 1 : 0,
        ide_workspace_authoring_host_pane_overlay_active(host) ? 1 : 0,
        buttons,
        (uint32_t)(sizeof(buttons) / sizeof(buttons[0])));
    for (i = 0u; i < count; ++i) {
        authoring_draw_button(renderer, palette, &buttons[i]);
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
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
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

static void authoring_draw_font_theme_placeholder(SDL_Renderer *renderer,
                                                  IDEWorkspaceAuthoringHost *host,
                                                  const IDEThemePalette *palette,
                                                  int viewport_width,
                                                  int viewport_height) {
    SDL_Rect panel;
    char detail[192];

    if (!renderer || !host || !palette) return;
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer,
                           palette->app_background.r,
                           palette->app_background.g,
                           palette->app_background.b,
                           238);
    SDL_Rect screen = {0, 0, viewport_width, viewport_height};
    SDL_RenderFillRect(renderer, &screen);

    panel.x = viewport_width / 12;
    panel.y = viewport_height / 10;
    panel.w = viewport_width - (panel.x * 2);
    panel.h = viewport_height / 3;
    SDL_SetRenderDrawColor(renderer,
                           palette->pane_body_fill.r,
                           palette->pane_body_fill.g,
                           palette->pane_body_fill.b,
                           245);
    SDL_RenderFillRect(renderer, &panel);
    SDL_SetRenderDrawColor(renderer,
                           palette->pane_border.r,
                           palette->pane_border.g,
                           palette->pane_border.b,
                           245);
    SDL_RenderDrawRect(renderer, &panel);

    authoring_draw_text(renderer, panel.x + 18, panel.y + 18, palette->text_primary,
                        "Font/Theme overlay pending IDEWA1-S3");
    snprintf(detail,
             sizeof(detail),
             "Current text zoom step: %d. Theme/font preview will wire through the existing IDE shared adapter next.",
             ide_shared_font_zoom_step());
    authoring_draw_text(renderer, panel.x + 18, panel.y + 50, palette->text_muted, detail);
}

void ide_workspace_authoring_overlay_render(IDECoreState *core,
                                            UIPane **panes,
                                            int pane_count,
                                            int viewport_width,
                                            int viewport_height) {
    RenderContext *ctx = getRenderContext();
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
    authoring_draw_controls(ctx->renderer, &core->workspaceAuthoring, &palette, viewport_width);
    if (ide_workspace_authoring_host_pane_overlay_active(&core->workspaceAuthoring)) {
        authoring_draw_pane_readout(ctx->renderer,
                                    &core->workspaceAuthoring,
                                    &palette,
                                    panes,
                                    pane_count);
    } else {
        authoring_draw_font_theme_placeholder(ctx->renderer,
                                              &core->workspaceAuthoring,
                                              &palette,
                                              viewport_width,
                                              viewport_height);
    }
}
