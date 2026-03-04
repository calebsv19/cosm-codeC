#include "engine/Render/render_helpers.h"

#include "ide/Panes/ToolPanels/tool_panel_chrome.h"
#include "ide/Panes/ToolPanels/tool_panel_top_layout.h"
#include "ide/Panes/ToolPanels/BuildOutput/render_tool_build_output.h"
#include "ide/Panes/ToolPanels/BuildOutput/tool_build_output.h"
#include "core/BuildSystem/build_diagnostics.h"
#include "ide/Panes/ToolPanels/BuildOutput/build_output_panel_state.h"
#include "ide/UI/row_surface.h"
#include "ide/UI/scroll_manager.h"
#include "ide/UI/shared_theme_font_adapter.h"
#include "engine/Render/render_pipeline.h" // getRenderContext
#include "engine/Render/render_font.h"
#include <SDL2/SDL.h>
#include <string.h>

// Forward decl from tool_build_output.c
bool build_output_is_selected(int idx);

static int build_output_rows_for_diag(const BuildDiagnostic* d) {
    if (!d) return 0;
    return d->notes[0] ? 3 : 2;
}

static TTF_Font* build_output_row_font(void) {
    TTF_Font* font = getUIFontByTier(CORE_FONT_TEXT_SIZE_CAPTION);
    return font ? font : getActiveFont();
}

static SDL_Color build_output_row_color(void) {
    IDEThemePalette palette = {0};
    SDL_Color color = {230, 230, 230, 255};
    if (ide_shared_theme_resolve_palette(&palette)) {
        color = palette.text_primary;
    }
    return color;
}

static void renderDiagnosticsList(const BuildDiagnostic* diags,
                                  size_t count,
                                  int x,
                                  int y,
                                  int maxY,
                                  int contentTop,
                                  int lineHeight,
                                  int highlightW,
                                  const SDL_Rect* clipRect,
                                  int mouseX,
                                  int mouseY) {
    char line[1400];
    TTF_Font* rowFont = build_output_row_font();
    SDL_Color rowColor = build_output_row_color();
    for (size_t i = 0; i < count; ++i) {
        const BuildDiagnostic* d = &diags[i];
        const char* sev = d->isError ? "[E]" : "[W]";
        int rowCount = d->notes[0] ? 3 : 2;
        int blockHeight = rowCount * lineHeight;
        if (y + blockHeight <= contentTop) {
            y += blockHeight;
            continue;
        }
        if (y >= maxY) {
            break;
        }
        bool isSelected = build_output_is_selected((int)i);
        SDL_Rect highlight = { x - 6, y - 2, highlightW, blockHeight };
        UIRowSurfaceLayout highlightSurface = ui_row_surface_layout_from_rect(highlight);
        UIRowSurfaceLayout visibleSurface = {0};
        if (ui_row_surface_clip(&highlightSurface, clipRect, &visibleSurface)) {
            ui_row_surface_render(getRenderContext()->renderer,
                                  &visibleSurface,
                                  &(UIRowSurfaceRenderSpec){
                                      .draw_selection_fill = isSelected,
                                      .draw_selection_outline = isSelected,
                                      .draw_hover_outline = ui_row_surface_contains(&visibleSurface, mouseX, mouseY)
                                  });
        }
        // Line 1: label + location
        snprintf(line, sizeof(line), "%s %s:%d:%d", sev, d->path, d->line, d->col);
        drawTextUTF8WithFontColorClipped(x, y, line, rowFont, rowColor, false, clipRect);
        y += lineHeight;
        // Line 2: indented message
        snprintf(line, sizeof(line), "    %s", d->message);
        drawTextUTF8WithFontColorClipped(x, y, line, rowFont, rowColor, false, clipRect);
        y += lineHeight;
        if (d->notes[0] && y + lineHeight <= maxY) {
            snprintf(line, sizeof(line), "    note: %s", d->notes);
            drawTextUTF8WithFontColorClipped(x, y, line, rowFont, rowColor, false, clipRect);
            y += lineHeight;
        }
    }
}

void renderBuildOutputPanel(UIPane* pane) {
    ToolPanelLayoutDefaults d = tool_panel_layout_defaults();
    ToolPanelSplitLayout split = {0};
    int contentTop = build_output_content_top(pane);
    tool_panel_compute_split_layout(pane, contentTop, &split);
    int x = split.body_rect.x + (d.pad_left - 1);
    int y = build_output_first_row_y(pane);
    int maxY = split.body_rect.y + split.body_rect.h;
    int lineHeight = BUILD_OUTPUT_LINE_HEIGHT;
    const int trackWidth = 6;
    const int trackPadding = 4;
    tool_panel_render_split_background(getRenderContext()->renderer, pane, contentTop, 14);

    PaneScrollState* scroll = build_output_get_scroll_state();
    scroll_state_set_viewport(scroll, (float)split.body_rect.h);

    size_t count = 0;
    const BuildDiagnostic* diags = build_diagnostics_get(&count);
    if (!diags || count == 0) {
        drawTextWithTier(x, y, "(No build diagnostics yet)", CORE_FONT_TEXT_SIZE_CAPTION);
        build_output_set_scroll_rects((SDL_Rect){0}, (SDL_Rect){0});
        return;
    }

    float contentHeight = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        contentHeight += (float)(build_output_rows_for_diag(&diags[i]) * lineHeight);
    }
    scroll_state_set_content_height(scroll,
                                    scroll_state_top_anchor_content_height(scroll, contentHeight));
    y -= (int)scroll_state_get_offset(scroll);

    SDL_Rect clipRect = {
        split.body_rect.x,
        split.body_rect.y,
        split.body_rect.w - (trackWidth + trackPadding),
        split.body_rect.h
    };
    if (clipRect.w < 0) clipRect.w = 0;

    int highlightW = clipRect.w - (x - clipRect.x) + 6;
    if (highlightW < 0) highlightW = 0;
    int mouseX = 0;
    int mouseY = 0;
    SDL_GetMouseState(&mouseX, &mouseY);
    pushClipRect(&clipRect);
    renderDiagnosticsList(diags,
                          count,
                          x,
                          y,
                          maxY,
                          split.body_rect.y,
                          lineHeight,
                          highlightW,
                          &clipRect,
                          mouseX,
                          mouseY);
    popClipRect();

    if (scroll_state_can_scroll(scroll) && split.body_rect.h > 0) {
        SDL_Rect track = {
            split.body_rect.x + split.body_rect.w - trackWidth,
            split.body_rect.y,
            trackWidth,
            split.body_rect.h
        };
        SDL_Rect thumb = scroll_state_thumb_rect(scroll,
                                                 track.x,
                                                 track.y,
                                                 track.w,
                                                 track.h);
        SDL_Color trackColor = scroll->track_color;
        SDL_Color thumbColor = scroll->thumb_color;
        SDL_SetRenderDrawColor(getRenderContext()->renderer, trackColor.r, trackColor.g, trackColor.b, trackColor.a);
        SDL_RenderFillRect(getRenderContext()->renderer, &track);
        SDL_SetRenderDrawColor(getRenderContext()->renderer, thumbColor.r, thumbColor.g, thumbColor.b, thumbColor.a);
        SDL_RenderFillRect(getRenderContext()->renderer, &thumb);
        build_output_set_scroll_rects(track, thumb);
    } else {
        build_output_set_scroll_rects((SDL_Rect){0}, (SDL_Rect){0});
    }
}
