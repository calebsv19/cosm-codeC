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

#include "core/TextSelection/text_selection_manager.h"

#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

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
    SDL_SetRenderDrawColor(renderer, 255, 210, 120, 180);
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
        HEADER_HEIGHT - 2
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

    EditorViewState* vs = getCoreState()->editorViewState;

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
    SDL_SetRenderDrawColor(renderer, 90, 90, 90, 255);
    SDL_Rect outline = { boxX, boxY, boxW, boxH };
    SDL_RenderDrawRect(renderer, &outline);

    SDL_Rect leafClip = { boxX + 1, boxY + 1, boxW - 2, boxH - 2 };
    if (leafClip.w < 0) leafClip.w = 0;
    if (leafClip.h < 0) leafClip.h = 0;
    pushClipRect(&leafClip);

    if (isDropTarget) {
        SDL_Rect dropRect = { boxX + 1, boxY + HEADER_HEIGHT + 1, boxW - 2, boxH - HEADER_HEIGHT - 2 };
        if (dropRect.w > 0 && dropRect.h > 0) {
            SDL_SetRenderDrawColor(renderer, 190, 220, 255, 15);
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
            SDL_Rect tabRect = { tabX, boxY, tabW, HEADER_HEIGHT };
            int tabYInset = (HEADER_HEIGHT - EDITOR_TAB_HEIGHT) / 2;
            if (tabYInset < 0) tabYInset = 0;
            SDL_Rect tabVisualRect = {
                tabRect.x,
                tabRect.y + tabYInset,
                tabRect.w,
                EDITOR_TAB_HEIGHT
            };

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
                SDL_SetRenderDrawColor(renderer, 100, 100, 200, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, HEADER_BG_R, HEADER_BG_G, HEADER_BG_B, 255);
            }
            SDL_RenderFillRect(renderer, &visible);

            int maxW = clamp_tab_width_for_index(view, i);
            int textBudget = maxW - (EDITOR_TAB_TEXT_PAD * 2);
            char display[256];
            build_tab_label(rawLabel, textBudget, tabFont, display, sizeof(display));
            int drawTextX = tabX + EDITOR_TAB_TEXT_PAD;
            int visibleTextW = (visible.x + visible.w) - drawTextX;
            if (visibleTextW > 0) {
                SDL_Color textColor = {255, 255, 255, 255};
                SDL_Rect textClip = visible;
                drawTextUTF8WithFontColorClipped(drawTextX, boxY + 2, display,
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
    SDL_SetRenderDrawColor(renderer, 66, 56, 56, 255);
    SDL_RenderFillRect(renderer, &xButtonVisualRect);

    // Border
    SDL_SetRenderDrawColor(renderer, 90, 80, 80, 255);
    SDL_RenderDrawRect(renderer, &xButtonVisualRect);

    // Draw "X" text
    int xTextW = getTextWidthWithFont("X", tabFont);
    int xTextX = xButtonVisualRect.x + (xButtonVisualRect.w - xTextW) / 2;
    int xTextY = xButtonVisualRect.y - 1;
    drawTextWithTier(xTextX, xTextY, "X", CORE_FONT_TEXT_SIZE_CAPTION);

    // Render buffer contents
    if (view->activeTab >= 0 && view->activeTab < view->fileCount) {
        OpenFile* active = view->openFiles[view->activeTab];
        if (active && active->buffer) {
            setEditorVerticalPaddingIfUnset(&active->state, EDITOR_CONTENT_TOP_PADDING);

            if (use_projection_render_source(active)) {
                int badgeX = boxX + EDITOR_LINE_NUMBER_GUTTER_W + 8;
                SDL_Rect badge = { badgeX, boxY + HEADER_HEIGHT + 3, 112, 14 };
                SDL_SetRenderDrawColor(renderer, 58, 64, 84, 200);
                SDL_RenderFillRect(renderer, &badge);
                drawTextWithTier(badge.x + 5, badge.y + 1, "projection view", CORE_FONT_TEXT_SIZE_CAPTION);
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
    
    EditorState* state = &file->state;
    int totalLines = editor_render_line_count(file);
    int lineHeight = EDITOR_LINE_HEIGHT;
    
    int boxX = view->x + EDITOR_PADDING;
    int boxY = view->y + EDITOR_PADDING;
    int boxW = view->w - 2 * EDITOR_PADDING;
    int boxH = view->h - 2 * EDITOR_PADDING;
    
    int contentHeight = boxH - HEADER_HEIGHT;
    int visibleLines = contentHeight / lineHeight;
    
    if (totalLines <= visibleLines) return;
    
    int maxScroll = totalLines - visibleLines;
    float scrollYRatio = (float)state->viewTopRow / (float)(maxScroll > 0 ? maxScroll : 1);
    float visibleRatio = (float)visibleLines / (float)totalLines;
    
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
    SDL_SetRenderDrawColor(renderer, 45, 45, 45, 150);
    SDL_RenderFillRect(renderer, &scrollbarTrack);

    if (editor_has_match_markers(file) && file->buffer->lineCount > 0) {
        int totalRealLines = file->buffer->lineCount;
        SDL_SetRenderDrawColor(renderer, 255, 210, 120, 170);
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
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 220);
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

    const int lineHeight = EDITOR_LINE_HEIGHT;
    TTF_Font* textFont = get_editor_text_font();
    const int gutterW = EDITOR_LINE_NUMBER_GUTTER_W;
    const int textX = x + gutterW + 6;
    const int startY = y + state->verticalPadding;
    const int contentHeight = h - (startY - y);
    const int maxVisibleLines = (contentHeight > 0) ? contentHeight / lineHeight : 0;
    const int totalLines = editor_render_line_count(file);
    SDL_Rect contentClip = { x, y, w, h };
    if (contentClip.w < 0) contentClip.w = 0;
    if (contentClip.h < 0) contentClip.h = 0;
    pushClipRect(&contentClip);

    SDL_Rect gutterRect = { x, y, gutterW, h };
    SDL_SetRenderDrawColor(renderer, 38, 40, 46, 220);
    SDL_RenderFillRect(renderer, &gutterRect);
    SDL_SetRenderDrawColor(renderer, 66, 68, 76, 255);
    SDL_RenderDrawLine(renderer, x + gutterW - 1, y, x + gutterW - 1, y + h);

    // Scroll bounding
    if (!projectionMode) {
        if (state->cursorRow < state->viewTopRow) {
            state->viewTopRow = state->cursorRow;
        } else if (state->cursorRow >= state->viewTopRow + maxVisibleLines) {
            state->viewTopRow = state->cursorRow - maxVisibleLines + 1;
        }
    }

    if (state->viewTopRow < 0) state->viewTopRow = 0;
    if (totalLines < maxVisibleLines) state->viewTopRow = 0;
    if (state->viewTopRow > totalLines - maxVisibleLines)
        state->viewTopRow = totalLines - maxVisibleLines;
    if (state->viewTopRow < 0) state->viewTopRow = 0;

    // Draw visible lines
    for (int i = 0; i < maxVisibleLines; i++) {
        int bufferLineIndex = state->viewTopRow + i;
        if (bufferLineIndex >= totalLines) break;

        const char* line = editor_render_line_at(file, bufferLineIndex);

        int yLine = startY + i * lineHeight;
        int maxWidth = w - gutterW - 14;
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
                                 ? (SDL_Color){165, 170, 180, 255}
                                 : (SDL_Color){130, 135, 145, 255};
            SDL_Rect numClip = { x + 1, yLine, gutterW - 2, lineHeight };
            drawTextUTF8WithFontColorClipped(numX, yLine, numBuf, textFont, numColor, false, &numClip);
        }

        if (!projectionMode) {
            int selStart, selEnd;
            if (isLineInSelection(bufferLineIndex, &selStart, &selEnd, buffer, state)) {
                int selXStart = textX + getTextWidthNWithFont(line, selStart, textFont);
                int selXEnd = textX + getTextWidthNWithFont(line, selEnd, textFont);
                if (selXStart < visibleXMin) selXStart = visibleXMin;
                if (selXEnd > visibleXMax) selXEnd = visibleXMax;
                SDL_Rect highlight = { selXStart, yLine, selXEnd - selXStart, lineHeight };
                if (highlight.w > 0) {
                    SDL_SetRenderDrawColor(renderer, 80, 120, 200, 100);
                    SDL_RenderFillRect(renderer, &highlight);
                }
            }
        }

        SDL_Color textColor = {255, 255, 255, 255};
        SDL_Rect lineClip = { textX, yLine, maxWidth, lineHeight };
        drawTextUTF8WithFontColorClipped(textX, yLine, line, textFont, textColor, false, &lineClip);

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

                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                SDL_RenderDrawLine(renderer, cursorX, yLine, cursorX, yLine + lineHeight);
            }
        }
    }
    popClipRect();
}
