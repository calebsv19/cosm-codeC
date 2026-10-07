#include "render_helpers.h"
#include "render_pipeline.h"
#include "render_font.h"
#include "render_text_helpers.h"
#include "ide/UI/ide_ui_button.h"
#include "ide/UI/panel_control_widgets.h"
#include "ide/UI/panel_button_scope.h"

/* The IDE uses a Vulkan SDL-compatibility renderer. Do not pass that pointer to
 * kit_ui's direct SDL adapter, which requires an actual SDL_Renderer. */
static void paint_button(SDL_Rect rect, const char* label, KitUiButtonState state,
                         CoreFontTextSizeTier tier, const UIPanelCompactButtonSpec* custom) {
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer || rect.w <= 0 || rect.h <= 0) return;
    IDEThemePalette palette;
    KitUiButtonTheme theme;
    KitUiButtonSpec spec;
    KitUiButtonStyle style;
    KitUiButtonAppearance appearance;
    KitUiButtonLayout layout;
    ide_ui_button_resolve_palette(&palette);
    ide_ui_button_theme_from_palette(&palette, &theme);
    kit_ui_button_spec_init(&spec, label ? label : "");
    spec.state = state;
    if (!kit_ui_button_style_resolve(&theme, &spec, &style) ||
        !ide_ui_button_compact_appearance(&appearance, &layout)) return;
    if (custom && custom->use_custom_fill)
        style.fill = (CoreThemeColor){custom->custom_fill.r, custom->custom_fill.g, custom->custom_fill.b, custom->custom_fill.a};
    if (custom && custom->use_custom_outline)
        style.outline = (CoreThemeColor){custom->custom_outline.r, custom->custom_outline.g, custom->custom_outline.b, custom->custom_outline.a};
    KitRenderRect bounds = {(float)rect.x, (float)rect.y, (float)rect.w, (float)rect.h};
    KitRenderRect inner = kit_ui_rect_inset(bounds, appearance.border_thickness);
    float radius = kit_ui_corner_radius_clamp(appearance.corner_radius, bounds.width, bounds.height);
    pushClipRect(&rect);
    SDL_SetRenderDrawColor(ctx->renderer, style.outline.r, style.outline.g, style.outline.b, style.outline.a);
    SDL_FRect outer_rect = {bounds.x, bounds.y, bounds.width, bounds.height};
    vk_renderer_fill_rounded_rect(ctx->renderer, &outer_rect, radius);
    SDL_SetRenderDrawColor(ctx->renderer, style.fill.r, style.fill.g, style.fill.b, style.fill.a);
    SDL_FRect inner_rect = {inner.x, inner.y, inner.width, inner.height};
    vk_renderer_fill_rounded_rect(ctx->renderer, &inner_rect,
                                  kit_ui_corner_radius_for_inset(radius, appearance.border_thickness));
    TTF_Font* font = getUIFontByTier(tier);
    if (!font) font = getActiveFont();
    int width = getTextWidthUTF8WithFont(spec.label, font);
    int height = font ? TTF_FontHeight(font) : 12;
    SDL_Rect text_clip = {rect.x + 4, rect.y + 2, rect.w - 8, rect.h - 4};
    if (custom && custom->close_glyph) {
        int half=(rect.w<rect.h?rect.w:rect.h)/4;
        if(half<2)half=2;
        int cx=rect.x+rect.w/2,cy=rect.y+rect.h/2;
        SDL_SetRenderDrawColor(ctx->renderer,style.text.r,style.text.g,style.text.b,style.text.a);
        SDL_RenderDrawLine(ctx->renderer,cx-half,cy-half,cx+half,cy+half);
        SDL_RenderDrawLine(ctx->renderer,cx-half,cy+half,cx+half,cy-half);
    } else if (text_clip.w > 0 && text_clip.h > 0) {
        int x = rect.x + (rect.w - width) / 2;
        if (x < text_clip.x) x = text_clip.x;
        drawTextUTF8WithFontColorClipped(x, rect.y + (rect.h - height) / 2, spec.label, font,
            (SDL_Color){style.text.r, style.text.g, style.text.b, style.text.a}, false, &text_clip);
    }
    if (state.focused && rect.w > 16 && rect.h > 4) {
        SDL_SetRenderDrawColor(ctx->renderer, style.outline.r, style.outline.g, style.outline.b, style.outline.a);
        SDL_RenderDrawLine(ctx->renderer, rect.x + 8, rect.y + rect.h - 4,
                           rect.x + rect.w - 8, rect.y + rect.h - 4);
    }
    popClipRect();
}

void renderSharedButton(struct UIPane* pane, SDL_Rect rect, const char* label, KitUiButtonState state) {
    (void)pane;
    paint_button(rect, label, state, CORE_FONT_TEXT_SIZE_CAPTION, NULL);
}
void renderSharedPanelButton(SDL_Renderer* renderer, const UIPanelCompactButtonSpec* spec) {
    RenderContext* ctx = getRenderContext();
    if (!spec || !ctx || renderer != ctx->renderer) return;
    KitUiButtonState state = {.hovered = spec->hovered, .selected = spec->active,
        .pressed = spec->pressed, .disabled = spec->disabled, .focused = spec->outlined};
    (void)ide_panel_buttons_paint_state(spec->rect,&state);
    paint_button(spec->rect, spec->label, state, spec->tier, spec);
}
