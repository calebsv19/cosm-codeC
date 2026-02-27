#include "engine/Render/render_pipeline.h"
#include "engine/Render/render_helpers.h"              // renderUIPane, drawText
#include "engine/Render/render_text_helpers.h"    
#include "engine/Render/render_font.h"

#include "ide/Panes/Editor/Render/render_editor.h"
#include "app/GlobalInfo/system_control.h"
#include "app/GlobalInfo/core_state.h"

#include "ide/Panes/PaneInfo/pane.h"
#include "ide/Panes/Editor/editor_view.h"    // editorView, layout, render
#include "ide/Panes/Editor/editor_view_state.h"
#include "ide/Panes/Editor/editor_core.h"
#include "ide/Panes/ControlPanel/control_panel.h"
#include "ide/UI/shared_theme_font_adapter.h"
#include "ide/UI/ui_selection_style.h"

#include "core/TextSelection/text_selection_manager.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static Uint8 clamp_u8(int value) {
    if (value < 0) return 0;
    if (value > 255) return 255;
    return (Uint8)value;
}

static SDL_Color brighten_color(SDL_Color c, int amount) {
    return (SDL_Color){
        clamp_u8((int)c.r + amount),
        clamp_u8((int)c.g + amount),
        clamp_u8((int)c.b + amount),
        c.a
    };
}

static SDL_Color darken_color(SDL_Color c, int amount) {
    return (SDL_Color){
        clamp_u8((int)c.r - amount),
        clamp_u8((int)c.g - amount),
        clamp_u8((int)c.b - amount),
        c.a
    };
}

static SDL_Color alpha_color(SDL_Color c, Uint8 alpha) {
    c.a = alpha;
    return c;
}

static int color_luma(SDL_Color c) {
    return ((int)c.r * 299 + (int)c.g * 587 + (int)c.b * 114) / 1000;
}

static bool is_light_color(SDL_Color c) {
    return color_luma(c) >= 170;
}

static SDL_Color editor_gutter_fill(const IDEThemePalette* palette);

static SDL_Color editor_content_text_color(const IDEThemePalette* palette) {
    if (!palette) return (SDL_Color){235, 235, 235, 255};
    if (is_light_color(palette->app_background)) {
        return darken_color(palette->text_muted, 18);
    }
    return palette->text_primary;
}

static SDL_Color editor_tab_active_fill(const IDEThemePalette* palette) {
    if (!palette) return (SDL_Color){48, 48, 56, 255};
    return palette->app_background;
}

static SDL_Color editor_tab_inactive_fill(const IDEThemePalette* palette) {
    SDL_Color header = {44, 46, 50, 255};
    if (palette) {
        header = editor_gutter_fill(palette);
        header.a = 255;
    }
    if (is_light_color(header)) {
        return darken_color(header, 10);
    }
    return brighten_color(header, 10);
}

static SDL_Color editor_header_fill(const IDEThemePalette* palette) {
    SDL_Color fill = editor_gutter_fill(palette);
    fill.a = 255;
    return fill;
}

static SDL_Color editor_content_fill(const IDEThemePalette* palette) {
    if (!palette) return (SDL_Color){28, 30, 34, 255};
    return palette->app_background;
}

static SDL_Color editor_gutter_fill(const IDEThemePalette* palette) {
    if (!palette) return (SDL_Color){34, 36, 40, 220};
    if (is_light_color(palette->app_background)) {
        return alpha_color(darken_color(palette->app_background, 14), 220);
    }
    return alpha_color(darken_color(palette->pane_body_fill, 6), 220);
}

static bool use_projection_render_source(const OpenFile* file) {
    return file &&
           editor_file_projection_active(file) &&
           file->projection.lines &&
           file->projection.lineCount > 0;
}

static int editor_render_line_count(const OpenFile* file) {
    if (!file) return 0;
    if (use_projection_render_source(file)) {
        return file->projection.lineCount;
    }
    if (!file->buffer) return 0;
    return file->buffer->lineCount;
}

static const char* editor_render_line_at(const OpenFile* file, int lineIndex) {
    if (!file || lineIndex < 0) return "";
    if (use_projection_render_source(file)) {
        if (lineIndex >= file->projection.lineCount) return "";
        return file->projection.lines[lineIndex] ? file->projection.lines[lineIndex] : "";
    }
    if (!file->buffer || lineIndex >= file->buffer->lineCount) return "";
    return file->buffer->lines[lineIndex] ? file->buffer->lines[lineIndex] : "";
}

static int editor_render_source_line_number(const OpenFile* file, int rowIndex) {
    if (!file || rowIndex < 0) return -1;
    if (use_projection_render_source(file)) {
        if (!file->projection.projectedToRealLine || rowIndex >= file->projection.lineCount) return -1;
        int real = file->projection.projectedToRealLine[rowIndex];
        if (real < 0) return -1;
        return real + 1;
    }
    if (!file->buffer || rowIndex >= file->buffer->lineCount) return -1;
    return rowIndex + 1;
}

static bool editor_has_match_markers(const OpenFile* file) {
    const char* query = control_panel_get_search_query();
    bool markerModeEnabled = control_panel_is_search_enabled() &&
                             control_panel_target_editor_enabled() &&
                             control_panel_get_editor_view_mode() == CONTROL_EDITOR_VIEW_MARKERS &&
                             query && query[0] != '\0';
    return markerModeEnabled &&
           file &&
           !use_projection_render_source(file) &&
           file->projection.realMatchLines &&
           file->projection.realMatchCount > 0 &&
           file->buffer &&
           file->buffer->lineCount > 0;
}

static void render_projection_inline_markers(EditorView* view, OpenFile* file) {
    if (!view || !file || !editor_has_match_markers(file)) return;

    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer* renderer = ctx->renderer;
    IDEThemePalette palette = {0};

    const int lineHeight = EDITOR_LINE_HEIGHT;
    int boxX = view->x + EDITOR_PADDING;
    int boxY = view->y + EDITOR_PADDING;
    int boxW = view->w - 2 * EDITOR_PADDING;
    int boxH = view->h - 2 * EDITOR_PADDING;
    int contentHeight = boxH - HEADER_HEIGHT;
    int visibleLines = (contentHeight > 0) ? contentHeight / lineHeight : 0;
    int totalRealLines = file->buffer->lineCount;
    if (visibleLines <= 0 || totalRealLines <= 0) return;
    if (totalRealLines > visibleLines) return;

    int markerX = boxX + boxW - 7;
    int startY = boxY + HEADER_HEIGHT + file->state.verticalPadding;
    ide_shared_theme_resolve_palette(&palette);
    SDL_SetRenderDrawColor(renderer,
                           palette.accent_warning.r,
                           palette.accent_warning.g,
                           palette.accent_warning.b,
                           180);
    for (int i = 0; i < file->projection.realMatchCount; ++i) {
        int line = file->projection.realMatchLines[i];
        if (line < 0 || line >= totalRealLines) continue;
        int y = startY + line * lineHeight + (lineHeight / 2) - 1;
        SDL_Rect mark = { markerX, y, 4, 2 };
        SDL_RenderFillRect(renderer, &mark);
    }
}

static SDL_Rect compute_tab_header_viewport(const EditorView* view) {
    int boxX = view->x + EDITOR_PADDING;
    int boxY = view->y + EDITOR_PADDING;
    int boxW = view->w - 2 * EDITOR_PADDING;
    int closeReserve = EDITOR_TAB_CLOSE_BTN_SIZE + EDITOR_TAB_CLOSE_BTN_MARGIN + 6;

    SDL_Rect viewport = {
        boxX + 1,
        boxY + 1,
        boxW - closeReserve - 2,
        HEADER_HEIGHT - 1
    };
    if (viewport.w < 0) viewport.w = 0;
    if (viewport.h < 0) viewport.h = 0;
    return viewport;
}

static TTF_Font* get_editor_tab_font(void) {
    TTF_Font* font = getUIFontByTier(CORE_FONT_TEXT_SIZE_CAPTION);
    return font ? font : getActiveFont();
}

static TTF_Font* get_editor_text_font(void) {
    TTF_Font* font = getUIFontByTier(CORE_FONT_TEXT_SIZE_CAPTION);
    return font ? font : getActiveFont();
}

static int clamp_tab_width_for_index(const EditorView* view, int tabIndex) {
    return (tabIndex == view->activeTab) ? EDITOR_TAB_MAX_W_ACTIVE : EDITOR_TAB_MAX_W_INACTIVE;
}

static void build_tab_label(const char* rawLabel, int maxTextW, TTF_Font* font, char* out, size_t outCap) {
    if (!out || outCap == 0) return;
    out[0] = '\0';
    if (!rawLabel || !rawLabel[0] || maxTextW <= 0) return;

    int rawW = getTextWidthWithFont(rawLabel, font);
    if (rawW <= maxTextW) {
        snprintf(out, outCap, "%s", rawLabel);
        return;
    }

    const char* ell = "...";
    int ellW = getTextWidthWithFont(ell, font);
    if (ellW > maxTextW) return;

    int budget = maxTextW - ellW;
    size_t keep = getTextClampedLengthWithFont(rawLabel, budget, font);
    if (keep > outCap - 4) keep = outCap - 4;

    memcpy(out, rawLabel, keep);
    memcpy(out + keep, ell, 3);
    out[keep + 3] = '\0';
}

static int compute_tab_visual_width(const EditorView* view, int tabIndex, const char* rawLabel, TTF_Font* font) {
    int maxW = clamp_tab_width_for_index(view, tabIndex);
    int textBudget = maxW - (EDITOR_TAB_TEXT_PAD * 2);
    if (textBudget < 0) textBudget = 0;

    char display[256];
    build_tab_label(rawLabel, textBudget, font, display, sizeof(display));
    int textW = getTextWidthWithFont(display, font);
    int tabW = textW + EDITOR_TAB_TEXT_PAD * 2;
    if (tabW < EDITOR_TAB_MIN_W) tabW = EDITOR_TAB_MIN_W;
    if (tabW > maxW) tabW = maxW;
    return tabW;
}

void renderEditorViewContents(UIPane* pane, bool hovered, IDECoreState* core) {
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
//    SDL_Renderer* renderer = ctx->renderer;

// renderUIPane(pane, hovered, core);

    if (!pane->editorView) {
        drawTextWithTier(pane->x + 8, pane->y + 30, "(empty editor)", CORE_FONT_TEXT_SIZE_CAPTION);
        return;
    }


    EditorViewState* vs = getCoreState()->editorViewState;
    vs->leafHitboxCount = 0;
    vs->viewTabHitboxCount = 0;
    vs->scrollThumbHitboxCount = 0;
    vs->splitDividerHitboxCount = 0;

    // Layout and render
    performEditorLayout(pane->editorView, pane->x, pane->y, pane->w, pane->h);
    renderEditorEntry(pane->editorView);  // uses core->activeEditorView internally
}


void renderEditorEntry(EditorView* view) {
    if (!view) return;

    EditorViewState* vs = getCoreState()->editorViewState;

    vs->viewTabHitboxCount = 0;

    if (!isEditorDraggingScrollbar()) {
        vs->scrollThumbHitboxCount = 0;
    }

    renderEditorViewRecursive(view);
}
        
    
void renderEditorViewRecursive(EditorView* view) {
    if (!view) return;
        
    if (view->type == VIEW_SPLIT) {
        layoutSplitChildren(view);
        addSplitDividerHitboxForView(view);
        renderEditorViewRecursive(view->childA);
        renderEditorViewRecursive(view->childB);
    } else if (view->type == VIEW_LEAF) {
        renderLeafEditorView(view);
    }
}


void renderLeafEditorView(EditorView* view) {
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer* renderer = ctx->renderer;
    IDEThemePalette palette = {0};
    SDL_Color bodyText = {0};
    SDL_Color mutedText = {0};
    SDL_Color headerFill = {0};
    SDL_Color contentFill = {0};
    SDL_Color activeTabFill = {0};
    SDL_Color inactiveTabFill = {0};

    EditorViewState* vs = getCoreState()->editorViewState;
    ide_shared_theme_resolve_palette(&palette);
    bodyText = editor_content_text_color(&palette);
    mutedText = palette.text_muted;
    if (is_light_color(palette.app_background)) {
        mutedText = darken_color(mutedText, 12);
    }
    headerFill = editor_header_fill(&palette);
    contentFill = editor_content_fill(&palette);
    activeTabFill = editor_tab_active_fill(&palette);
    inactiveTabFill = editor_tab_inactive_fill(&palette);

    int boxX = view->x + EDITOR_PADDING;
    int boxY = view->y + EDITOR_PADDING;
    int boxW = view->w - 2 * EDITOR_PADDING;
    int boxH = view->h - 2 * EDITOR_PADDING;

    // Register clickable region for this leaf editor
    if (vs->leafHitboxCount < MAX_LEAF_HITBOXES) {
        vs->leafHitboxes[vs->leafHitboxCount].rect = (SDL_Rect){ boxX, boxY, boxW, boxH };
        vs->leafHitboxes[vs->leafHitboxCount].view = view;
        vs->leafHitboxCount++;
    }

    ProjectDragState* drag = &getCoreState()->projectDrag;
    bool isDropTarget = drag->entry && drag->active && drag->validTarget && drag->targetView == view;

    EditorView* activeView = getCoreState()->activeEditorView;
    EditorView* hoveredView = getHoveredEditorView();

    SDL_Color hoverBorder = {0};
    SDL_Color activeBorder = {0};
    SDL_Color activeHoverBorder = {0};
    ide_shared_theme_editor_border_colors(&hoverBorder, &activeBorder, &activeHoverBorder);

    // Highlight active view
    if (view == activeView) {
        SDL_Color draw = (view == hoveredView) ? activeHoverBorder : activeBorder;
        SDL_SetRenderDrawColor(renderer,
                               draw.r, draw.g, draw.b, 255);
        SDL_Rect highlight = { boxX - 2, boxY - 2, boxW + 4, boxH + 4 };
        SDL_RenderDrawRect(renderer, &highlight);
    } else if (view == hoveredView) {
        SDL_SetRenderDrawColor(renderer,
                               hoverBorder.r, hoverBorder.g, hoverBorder.b, 255);
        SDL_Rect hoverOutline = { boxX - 1, boxY - 1, boxW + 2, boxH + 2 };
        SDL_RenderDrawRect(renderer, &hoverOutline);
    }

    // Draw editor outline
    SDL_SetRenderDrawColor(renderer,
                           palette.pane_border.r,
                           palette.pane_border.g,
                           palette.pane_border.b,
                           255);
    SDL_Rect outline = { boxX, boxY, boxW, boxH };
    SDL_RenderDrawRect(renderer, &outline);

    SDL_Rect leafClip = { boxX + 1, boxY + 1, boxW - 2, boxH - 2 };
    if (leafClip.w < 0) leafClip.w = 0;
    if (leafClip.h < 0) leafClip.h = 0;
    pushClipRect(&leafClip);

    SDL_Rect headerBgRect = { boxX + 1, boxY + 1, boxW - 2, HEADER_HEIGHT - 1 };
    if (headerBgRect.w > 0 && headerBgRect.h > 0) {
        SDL_SetRenderDrawColor(renderer, headerFill.r, headerFill.g, headerFill.b, headerFill.a);
        SDL_RenderFillRect(renderer, &headerBgRect);
    }

    SDL_Rect contentBgRect = { boxX + 1, boxY + HEADER_HEIGHT, boxW - 2, boxH - HEADER_HEIGHT - 1 };
    if (contentBgRect.w > 0 && contentBgRect.h > 0) {
        SDL_SetRenderDrawColor(renderer, contentFill.r, contentFill.g, contentFill.b, contentFill.a);
        SDL_RenderFillRect(renderer, &contentBgRect);
    }

    if (isDropTarget) {
        SDL_Rect dropRect = { boxX + 1, boxY + HEADER_HEIGHT + 1, boxW - 2, boxH - HEADER_HEIGHT - 2 };
        if (dropRect.w > 0 && dropRect.h > 0) {
            SDL_Color dropFill = alpha_color(palette.selection_fill, 32);
            SDL_SetRenderDrawColor(renderer, dropFill.r, dropFill.g, dropFill.b, dropFill.a);
            SDL_RenderFillRect(renderer, &dropRect);
        }
    }

    SDL_Rect tabViewport = compute_tab_header_viewport(view);
    TTF_Font* tabFont = get_editor_tab_font();
    int tabTotalW = 0;
    for (int i = 0; i < view->fileCount; ++i) {
        OpenFile* file = view->openFiles[i];
        const char* label = (file && file->filePath) ? getFileName(file->filePath) : "";
        int tabW = compute_tab_visual_width(view, i, label, tabFont);
        tabTotalW += tabW;
        if (i < view->fileCount - 1) tabTotalW += EDITOR_TAB_GAP;
    }
    int maxTabScroll = tabTotalW - tabViewport.w;
    if (maxTabScroll < 0) maxTabScroll = 0;
    if (view->tabScrollX < 0) view->tabScrollX = 0;
    if (view->tabScrollX > maxTabScroll) view->tabScrollX = maxTabScroll;

    if (tabViewport.w > 0 && tabViewport.h > 0) {
        int tabX = tabViewport.x - view->tabScrollX;
        pushClipRect(&tabViewport);
        for (int i = 0; i < view->fileCount; i++) {
            OpenFile* file = view->openFiles[i];
            if (!file || !file->filePath) continue;

            const char* rawLabel = getFileName(file->filePath);
            int tabW = compute_tab_visual_width(view, i, rawLabel, tabFont);
            SDL_Rect tabRect = { tabX, headerBgRect.y, tabW, headerBgRect.h };
            SDL_Rect tabVisualRect = tabRect;
            if (i != view->activeTab) {
                int tabYInset = (headerBgRect.h - EDITOR_TAB_HEIGHT) / 2;
                if (tabYInset < 1) tabYInset = 1;
                tabVisualRect.y += tabYInset;
                tabVisualRect.h = EDITOR_TAB_HEIGHT;
                if (tabVisualRect.y + tabVisualRect.h > headerBgRect.y + headerBgRect.h) {
                    tabVisualRect.h = (headerBgRect.y + headerBgRect.h) - tabVisualRect.y;
                }
                if (tabVisualRect.h < 8) tabVisualRect.h = 8;
            }

            SDL_Rect visible = {0};
            if (!SDL_IntersectRect(&tabVisualRect, &tabViewport, &visible)) {
                tabX += tabW + EDITOR_TAB_GAP;
                continue;
            }

            if (vs->viewTabHitboxCount < MAX_TAB_HITBOXES) {
                SDL_Rect hitVisible = {0};
                if (!SDL_IntersectRect(&tabRect, &tabViewport, &hitVisible)) {
                    hitVisible = visible;
                }
                vs->viewTabHitboxes[vs->viewTabHitboxCount].rect = hitVisible;
                vs->viewTabHitboxes[vs->viewTabHitboxCount].tabIndex = i;
                vs->viewTabHitboxes[vs->viewTabHitboxCount].view = view;
                vs->viewTabHitboxCount++;
            }

            if (i == view->activeTab) {
                SDL_SetRenderDrawColor(renderer, activeTabFill.r, activeTabFill.g, activeTabFill.b, 255);
            } else {
                SDL_SetRenderDrawColor(renderer,
                                       inactiveTabFill.r,
                                       inactiveTabFill.g,
                                       inactiveTabFill.b,
                                       inactiveTabFill.a);
            }
            SDL_RenderFillRect(renderer, &visible);

            int maxW = clamp_tab_width_for_index(view, i);
            int textBudget = maxW - (EDITOR_TAB_TEXT_PAD * 2);
            char display[256];
            build_tab_label(rawLabel, textBudget, tabFont, display, sizeof(display));
            int drawTextX = tabX + EDITOR_TAB_TEXT_PAD;
            int tabTextH = tabFont ? TTF_FontHeight(tabFont) : 12;
            int drawTextY = tabVisualRect.y + (tabVisualRect.h - tabTextH) / 2;
            if (drawTextY < tabVisualRect.y) drawTextY = tabVisualRect.y;
            int visibleTextW = (visible.x + visible.w) - drawTextX;
            if (visibleTextW > 0) {
                SDL_Color textColor = (i == view->activeTab) ? bodyText : mutedText;
                SDL_Rect textClip = visible;
                drawTextUTF8WithFontColorClipped(drawTextX, drawTextY, display,
                                                 tabFont, textColor, false,
                                                 &textClip);
            }

            tabX += tabW + EDITOR_TAB_GAP;
        }
        popClipRect();
    }

    // === Render corner "X" tab close button ===
    int xButtonSize = EDITOR_TAB_CLOSE_BTN_SIZE;
    int xButtonX = boxX + boxW - xButtonSize - EDITOR_TAB_CLOSE_BTN_MARGIN;
    int xButtonY = boxY + 4;

    SDL_Rect xButtonRect = { xButtonX, xButtonY, xButtonSize, xButtonSize };
    int visualInset = 3;
    if (xButtonRect.w < 10 || xButtonRect.h < 10) {
        visualInset = 1;
    }
    SDL_Rect xButtonVisualRect = {
        xButtonRect.x + visualInset,
        xButtonRect.y + visualInset,
        xButtonRect.w - visualInset * 2,
        xButtonRect.h - visualInset * 2
    };
    if (xButtonVisualRect.w < 6) xButtonVisualRect.w = 6;
    if (xButtonVisualRect.h < 6) xButtonVisualRect.h = 6;

    // Save into active file's editor state for input handling
    if (view->activeTab >= 0 && view->activeTab < view->fileCount) {
        OpenFile* file = view->openFiles[view->activeTab];
        if (file) {
            file->state.closeButtonRect = xButtonRect;
        }
    }

    // Subtle close button visual (smaller than hitbox)
    {
        SDL_Color closeFill = darken_color(palette.accent_error, 96);
        SDL_SetRenderDrawColor(renderer, closeFill.r, closeFill.g, closeFill.b, 255);
    }
    SDL_RenderFillRect(renderer, &xButtonVisualRect);

    // Border
    SDL_SetRenderDrawColor(renderer,
                           palette.accent_error.r,
                           palette.accent_error.g,
                           palette.accent_error.b,
                           255);
    SDL_RenderDrawRect(renderer, &xButtonVisualRect);

    // Draw "X" text
    int xTextW = getTextWidthWithFont("X", tabFont);
    int xTextX = xButtonVisualRect.x + (xButtonVisualRect.w - xTextW) / 2;
    int xTextY = xButtonVisualRect.y - 1;
    drawTextWithTierColor(xTextX, xTextY, "X", CORE_FONT_TEXT_SIZE_CAPTION, bodyText);

    // Render buffer contents
    if (view->activeTab >= 0 && view->activeTab < view->fileCount) {
        OpenFile* active = view->openFiles[view->activeTab];
        if (active && active->buffer) {
            setEditorVerticalPaddingIfUnset(&active->state, EDITOR_CONTENT_TOP_PADDING);

            if (use_projection_render_source(active)) {
                int badgeX = boxX + EDITOR_LINE_NUMBER_GUTTER_W + 8;
                SDL_Rect badge = { badgeX, boxY + HEADER_HEIGHT + 3, 112, 14 };
                SDL_Color badgeFill = alpha_color(brighten_color(palette.pane_body_fill, 16), 200);
                SDL_SetRenderDrawColor(renderer, badgeFill.r, badgeFill.g, badgeFill.b, badgeFill.a);
                SDL_RenderFillRect(renderer, &badge);
                drawTextWithTierColor(badge.x + 5,
                                      badge.y + 1,
                                      "projection view",
                                      CORE_FONT_TEXT_SIZE_CAPTION,
                                      bodyText);
            }

            renderEditorBuffer(
                active,
                &active->state,
                boxX,
                boxY + HEADER_HEIGHT,
                boxW,
                boxH - HEADER_HEIGHT
            );
            render_projection_inline_markers(view, active);
        }

        renderEditorScrollbar(view, active);
    }
    popClipRect();

}



void renderEditorScrollbar(EditorView* view, OpenFile* file) {
    if (!view || !file) return;
    
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer* renderer = ctx->renderer;
    IDEThemePalette palette = {0};
    SDL_Color trackFill = {0};
    SDL_Color thumbFill = {0};
    
    EditorState* state = &file->state;
    int totalLines = editor_render_line_count(file);
    int boxX = view->x + EDITOR_PADDING;
    int boxY = view->y + EDITOR_PADDING;
    int boxW = view->w - 2 * EDITOR_PADDING;
    int boxH = view->h - 2 * EDITOR_PADDING;
    
    int contentHeight = boxH - HEADER_HEIGHT;
    ide_shared_theme_resolve_palette(&palette);
    if (is_light_color(palette.app_background)) {
        trackFill = alpha_color(darken_color(palette.app_background, 12), 150);
    } else {
        trackFill = alpha_color(darken_color(palette.pane_body_fill, 8), 150);
    }
    thumbFill = alpha_color(brighten_color(palette.pane_border, 10), 220);
    
    if (contentHeight <= 0) return;

    float maxOffsetPx = editor_max_scroll_offset_px(state, totalLines, contentHeight);
    if (maxOffsetPx <= 0.0f) return;
    float offsetPx = state->scrollOffsetPx;
    if (offsetPx < 0.0f) offsetPx = 0.0f;
    if (offsetPx > maxOffsetPx) offsetPx = maxOffsetPx;
    float scrollYRatio = (maxOffsetPx > 0.0f) ? (offsetPx / maxOffsetPx) : 0.0f;
    float visibleRatio =
        (float)contentHeight / (float)editor_total_content_height_px(state, totalLines);
    
    // Clamp scrollYRatio
    if (scrollYRatio < 0.0f) scrollYRatio = 0.0f;
    if (scrollYRatio > 1.0f) scrollYRatio = 1.0f;
    
    int scrollbarWidth = SCROLLBAR_WIDTH;
    int scrollbarX = boxX + boxW - scrollbarWidth - SCROLLBAR_PADDING;
    
    int scrollThumbHeight = (int)(visibleRatio * contentHeight);
    if (scrollThumbHeight < 20) scrollThumbHeight = 20;
    
    int scrollThumbY = boxY + HEADER_HEIGHT +
                                (int)(scrollYRatio * (contentHeight - scrollThumbHeight));
                                
    SDL_Rect scrollbarTrack = { 
        scrollbarX, boxY + HEADER_HEIGHT,
        scrollbarWidth, contentHeight
    };

    SDL_Rect scrollbarThumb = {
        scrollbarX, scrollThumbY,
        scrollbarWidth, scrollThumbHeight
    };
      
    // Only add thumb hotbox if NOT currently dragging
    if (!isEditorDraggingScrollbar()) {
        addScrollThumbHitbox(scrollbarThumb, view, view->parentPane);
    } else {
        // Optional: Update dragging thumb rect position if needed during drag
        // e.g. updateScrollThumbPosition(view, scrollbarThumb);
    }
     
    // Render track background
    SDL_SetRenderDrawColor(renderer, trackFill.r, trackFill.g, trackFill.b, trackFill.a);
    SDL_RenderFillRect(renderer, &scrollbarTrack);

    if (editor_has_match_markers(file) && file->buffer->lineCount > 0) {
        int totalRealLines = file->buffer->lineCount;
        SDL_SetRenderDrawColor(renderer,
                               palette.accent_warning.r,
                               palette.accent_warning.g,
                               palette.accent_warning.b,
                               170);
        for (int i = 0; i < file->projection.realMatchCount; ++i) {
            int line = file->projection.realMatchLines[i];
            if (line < 0 || line >= totalRealLines) continue;
            float ratio = (float)line / (float)totalRealLines;
            int tickY = scrollbarTrack.y + (int)(ratio * (float)scrollbarTrack.h);
            SDL_Rect tick = { scrollbarTrack.x + 1, tickY, scrollbarTrack.w - 2, 2 };
            SDL_RenderFillRect(renderer, &tick);
        }
    }
    
    // Render thumb
    SDL_SetRenderDrawColor(renderer, thumbFill.r, thumbFill.g, thumbFill.b, thumbFill.a);
    SDL_RenderFillRect(renderer, &scrollbarThumb);
}



void renderEditorBuffer(OpenFile* file, EditorState* state,
                        int x, int y, int w, int h) {
    if (!file || !state) return;
    EditorBuffer* buffer = file->buffer;
    const bool projectionMode = use_projection_render_source(file);
    if (!projectionMode && !buffer) return;

    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer* renderer = ctx->renderer;
    IDEThemePalette palette = {0};
    SDL_Color gutterFill = {0};
    SDL_Color bodyText = {0};

    const int lineHeight = EDITOR_LINE_HEIGHT;
    TTF_Font* textFont = get_editor_text_font();
    const int gutterW = EDITOR_LINE_NUMBER_GUTTER_W;
    const int textX = x + gutterW + EDITOR_TEXT_LEFT_INSET;
    const int textMaxWidth = w - gutterW - 14;
    const int totalLines = editor_render_line_count(file);
    ide_shared_theme_resolve_palette(&palette);
    gutterFill = editor_gutter_fill(&palette);
    bodyText = editor_content_text_color(&palette);
    SDL_Rect contentClip = { x, y, w, h };
    if (contentClip.w < 0) contentClip.w = 0;
    if (contentClip.h < 0) contentClip.h = 0;
    pushClipRect(&contentClip);

    SDL_Rect gutterRect = { x, y, gutterW, h };
    SDL_SetRenderDrawColor(renderer, gutterFill.r, gutterFill.g, gutterFill.b, gutterFill.a);
    SDL_RenderFillRect(renderer, &gutterRect);

    SDL_Rect gutterTextClip = { x + 1, y, gutterW - 2, h };
    if (gutterTextClip.w < 0) gutterTextClip.w = 0;
    if (gutterTextClip.h < 0) gutterTextClip.h = 0;

    SDL_Rect textViewportClip = { textX, y, textMaxWidth, h };
    if (textViewportClip.w < 0) textViewportClip.w = 0;
    if (textViewportClip.h < 0) textViewportClip.h = 0;

    float maxScrollPx = editor_max_scroll_offset_px(state, totalLines, h);

    bool cursorChangedForScroll = (state->cursorRow != state->lastScrollAnchorCursorRow) ||
                                  (state->cursorCol != state->lastScrollAnchorCursorCol);
    if (!projectionMode && cursorChangedForScroll) {
        int viewportTopPx = (int)state->scrollTargetPx;
        int viewportBottomPx = viewportTopPx + h;
        int cursorTopPx = editor_vertical_padding_px(state) + state->cursorRow * lineHeight;
        int cursorBottomPx = cursorTopPx + lineHeight;
        if (cursorTopPx < viewportTopPx) {
            state->scrollTargetPx = (float)cursorTopPx;
        } else if (cursorBottomPx > viewportBottomPx) {
            state->scrollTargetPx = (float)(cursorBottomPx - h);
        }
    }

    if (state->scrollTargetPx < 0.0f) state->scrollTargetPx = 0.0f;
    if (state->scrollTargetPx > maxScrollPx) state->scrollTargetPx = maxScrollPx;
    if (state->scrollOffsetPx < 0.0f) state->scrollOffsetPx = 0.0f;
    if (state->scrollOffsetPx > maxScrollPx) state->scrollOffsetPx = maxScrollPx;

    state->scrollOffsetPx = state->scrollTargetPx;

    int firstVisibleRow = editor_first_visible_row(state);
    if (firstVisibleRow >= totalLines && totalLines > 0) {
        firstVisibleRow = totalLines - 1;
    }
    int intraOffset = editor_first_visible_row_offset_px(state, firstVisibleRow);
    state->viewTopRow = firstVisibleRow;
    state->lastScrollAnchorCursorRow = state->cursorRow;
    state->lastScrollAnchorCursorCol = state->cursorCol;

    // Draw visible lines
    for (int bufferLineIndex = firstVisibleRow; bufferLineIndex < totalLines; ++bufferLineIndex) {
        const char* line = editor_render_line_at(file, bufferLineIndex);
        int yLine = y - intraOffset + (bufferLineIndex - firstVisibleRow) * lineHeight;
        if (yLine >= y + h) break;
        int maxWidth = textMaxWidth;
        if (maxWidth < 0) maxWidth = 0;
        int visibleXMin = textX;
        int visibleXMax = textX + maxWidth;

        int lineNumber = editor_render_source_line_number(file, bufferLineIndex);
        if (lineNumber > 0) {
            char numBuf[24];
            snprintf(numBuf, sizeof(numBuf), "%d", lineNumber);
            int numW = getTextWidthWithFont(numBuf, textFont);
            int numX = x + gutterW - 6 - numW;
            if (numX < x + 2) numX = x + 2;
            SDL_Color numColor = projectionMode
                                 ? brighten_color(palette.text_muted, 18)
                                 : palette.text_muted;
            if (is_light_color(palette.app_background)) {
                numColor = darken_color(numColor, 16);
            }
            drawTextUTF8WithFontColorClipped(numX, yLine, numBuf, textFont, numColor, false, &gutterTextClip);
        }

        if (!projectionMode) {
            int selStart, selEnd;
            if (isLineInSelection(bufferLineIndex, &selStart, &selEnd, buffer, state)) {
                int selXStart = textX + getTextWidthNWithFont(line, selStart, textFont);
                int selXEnd = textX + getTextWidthNWithFont(line, selEnd, textFont);
                if (selXStart < visibleXMin) selXStart = visibleXMin;
                if (selXEnd > visibleXMax) selXEnd = visibleXMax;
                SDL_Rect highlight = { selXStart, yLine, selXEnd - selXStart, lineHeight };
                if (highlight.w <= 0) {
                    highlight.w = 2;
                }
                if (highlight.w > 0) {
                    SDL_Color sel = palette.selection_fill;
                    SDL_SetRenderDrawColor(renderer, sel.r, sel.g, sel.b, sel.a);
                    SDL_RenderFillRect(renderer, &highlight);
                }
            }
        }

        SDL_Color textColor = bodyText;
        drawTextUTF8WithFontColorClipped(textX, yLine, line, textFont, textColor, false, &textViewportClip);

        if (!projectionMode) {
            int lineLen = line ? (int)strlen(line) : 0;
            int lineWidth = getTextWidthWithFont(line, textFont);
            if (lineWidth <= 0) {
                lineWidth = maxWidth;
            }
            if (lineWidth > maxWidth) lineWidth = maxWidth;
            if (lineWidth < 0) lineWidth = 0;
            TextSelectionRect rect = {
                .bounds = { textX, yLine, lineWidth, lineHeight },
                .line = bufferLineIndex,
                .column_start = 0,
                .column_end = lineLen
            };
            TextSelectionDescriptor desc = {
                .owner = buffer,
                .owner_role = PANE_ROLE_EDITOR,
                .text = line,
                .text_length = (size_t)lineLen,
                .rects = &rect,
                .rect_count = 1,
                .flags = TEXT_SELECTION_FLAG_SELECTABLE,
                .copy_handler = NULL,
                .copy_user_data = NULL
            };
            text_selection_manager_register(&desc);

            // Always draw cursor on the current line, even if it's empty
            if (bufferLineIndex == state->cursorRow) {
                int cursorX = (line[0] == '\0')
                            ? textX
                            : textX + getTextWidthNWithFont(line, state->cursorCol, textFont);

                SDL_SetRenderDrawColor(renderer, bodyText.r, bodyText.g, bodyText.b, bodyText.a);
                SDL_RenderDrawLine(renderer, cursorX, yLine, cursorX, yLine + lineHeight);
            }
        }
    }
    popClipRect();
}
