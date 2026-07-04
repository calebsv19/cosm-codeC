#include "ide/Panes/ToolPanels/Errors/render_tool_errors.h"
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_font.h"

#include "ide/Panes/ToolPanels/Errors/tool_errors.h"
#include "ide/Panes/ToolPanels/Errors/errors_diagnostic_detail.h"
#include "ide/Panes/ToolPanels/tool_panel_chrome.h"
#include "ide/Panes/ToolPanels/tool_panel_top_layout.h"
#include "core/Diagnostics/diagnostics_engine.h"
#include "core/Analysis/analysis_refresh_view.h"
#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/project.h"
#include "engine/Render/render_pipeline.h"
#include "ide/UI/row_surface.h"
#include "ide/UI/scroll_manager.h"
#include "engine/Render/render_text_helpers.h"
#include "ide/UI/panel_text_field.h"
#include "ide/UI/shared_theme_font_adapter.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

// Forward from tool_errors.c
bool is_error_selected(int idx);
TTF_Font* get_error_font(void);
void errors_get_layout_metrics(const UIPane* pane, int* contentTop, int* headerHeight, int* diagHeight, int* lineHeight);

static const char* error_display_path(const char* rawPath, char* outBuf, size_t outCap) {
    if (!rawPath || !rawPath[0]) return "(unknown file)";
    if (!outBuf || outCap == 0) return rawPath;

    const char* roots[2] = { getWorkspacePath(), projectPath };
    for (int i = 0; i < 2; ++i) {
        const char* root = roots[i];
        if (!root || !root[0]) continue;
        size_t rootLen = strlen(root);
        if (strncmp(rawPath, root, rootLen) != 0) continue;

        const char* rel = rawPath + rootLen;
        if (*rel == '/' || *rel == '\\') rel++;
        if (!rel[0]) break;

        snprintf(outBuf, outCap, "%s", rel);
        return outBuf;
    }

    snprintf(outBuf, outCap, "%s", rawPath);
    return outBuf;
}

static int button_text_cap(const char* const* labels, int count) {
    int maxW = 28;
    for (int i = 0; i < count; ++i) {
        const char* s = labels[i] ? labels[i] : "";
        int w = getTextWidth(s) + 16;
        if (w > maxW) maxW = w;
    }
    return maxW;
}

static void render_detail_line(SDL_Rect* clip,
                               int x,
                               int* y,
                               int lineHeight,
                               const char* text,
                               SDL_Color color,
                               TTF_Font* font) {
    if (!clip || !y || !text) return;
    drawTextUTF8WithFontColorClipped(x,
                                     *y,
                                     text,
                                     font ? font : getActiveFont(),
                                     color,
                                     false,
                                     clip);
    *y += lineHeight;
}

static bool detail_has_room(const SDL_Rect* clip, int y, int lineHeight) {
    return clip && y + lineHeight <= clip->y + clip->h;
}

static void render_selected_diagnostic_detail(UIPane* pane,
                                              const Diagnostic* diag,
                                              TTF_Font* font,
                                              int lineHeight) {
    if (!pane || !diag) return;
    SDL_Rect rect = {0};
    if (!errors_get_detail_panel_rect(pane, &rect)) return;

    SDL_Renderer* renderer = getRenderContext()->renderer;
    SDL_SetRenderDrawColor(renderer, 20, 30, 38, 248);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, 70, 96, 118, 255);
    SDL_RenderDrawRect(renderer, &rect);

    SDL_Rect clip = {
        rect.x + 8,
        rect.y + 6,
        rect.w - 16,
        rect.h - 12
    };
    if (clip.w < 1 || clip.h < 1) return;

    char pathBuf[1024];
    const char* displayPath = error_display_path(diag->filePath, pathBuf, sizeof(pathBuf));

    SDL_Color titleColor = {235, 244, 250, 255};
    SDL_Color metaColor = {174, 202, 220, 255};
    SDL_Color textColor = {222, 230, 236, 255};
    SDL_Color hintColor = {202, 220, 176, 255};
    SDL_Color contextColor = {154, 180, 196, 255};
    int x = clip.x;
    int y = clip.y;

    ErrorsDiagnosticDetailModel model;
    if (!errors_diagnostic_detail_build(diag, displayPath, &model)) return;
    for (int i = 0; i < model.lineCount; ++i) {
        if (!detail_has_room(&clip, y, lineHeight)) break;
        const ErrorsDiagnosticDetailLine* line = &model.lines[i];
        SDL_Color color = textColor;
        switch (line->kind) {
            case ERRORS_DIAGNOSTIC_DETAIL_LINE_TITLE:
                color = titleColor;
                break;
            case ERRORS_DIAGNOSTIC_DETAIL_LINE_META:
            case ERRORS_DIAGNOSTIC_DETAIL_LINE_NAV_CONTEXT:
                color = metaColor;
                break;
            case ERRORS_DIAGNOSTIC_DETAIL_LINE_HINT:
            case ERRORS_DIAGNOSTIC_DETAIL_LINE_UNITS:
                color = hintColor;
                break;
            case ERRORS_DIAGNOSTIC_DETAIL_LINE_EXPLANATION:
            case ERRORS_DIAGNOSTIC_DETAIL_LINE_CONTEXT:
                color = contextColor;
                break;
            case ERRORS_DIAGNOSTIC_DETAIL_LINE_MESSAGE:
            default:
                color = textColor;
                break;
        }
        render_detail_line(&clip, x, &y, lineHeight, line->text, color, font);
    }
}

static void layout_errors_controls(const UIPane* pane,
                                   int* outTitle1X,
                                   int* outTitle2X,
                                   int* outTitle1Y,
                                   int* outTitle2Y,
                                   SDL_Rect* outAll,
                                   SDL_Rect* outErrors,
                                   SDL_Rect* outWarnings,
                                   SDL_Rect* outOpenAll,
                                   SDL_Rect* outCloseAll,
                                   UIPanelTextFieldButtonStripLayout* outSearch) {
    ToolPanelLayoutDefaults d = tool_panel_layout_defaults();
    const int padX = d.pad_left;
    const int rowX = pane->x + padX;
    const int topY = pane->y + d.controls_top;
    const int rowGap = d.row_gap;
    const int buttonH = d.button_h;
    int gapX = 5;
    const int titleGap = 7;
    const char* title1 = "Show";
    const char* title2 = "Batch";
    int titleColW = getTextWidth(title1);
    int t2w = getTextWidth(title2);
    if (t2w > titleColW) titleColW = t2w;
    titleColW += 8;
    int areaW = pane->w - (padX * 2);
    if (areaW < 1) areaW = 1;
    int buttonColW = areaW - titleColW - titleGap;
    if (buttonColW < 1) buttonColW = 1;

    // Match control-panel feel: shared total width for both rows; different row counts derive per-row button widths.
    const int densestCount = 3;   // All/Errors/Warnings
    const int smallestCount = 2;  // Open All/Close All
    const char* row1Labels[] = { "All", "Errors", "Warnings" };
    const char* row2Labels[] = { "Open All", "Close All" };

    int sharedTotalW = buttonColW;

    int row1BtnCap = button_text_cap(row1Labels, 3);
    int row2BtnCap = button_text_cap(row2Labels, 2);
    int row1TotalCap = row1BtnCap * densestCount + gapX * (densestCount - 1);
    int row2TotalCap = row2BtnCap * smallestCount + gapX * (smallestCount - 1);
    int freezeCap = (row1TotalCap < row2TotalCap) ? row1TotalCap : row2TotalCap;
    freezeCap = (freezeCap * 7) / 4; // ~1.75x larger max growth before freezing
    if (freezeCap > buttonColW) freezeCap = buttonColW;
    if (sharedTotalW > freezeCap) sharedTotalW = freezeCap;

    int bw1 = (sharedTotalW - gapX * (densestCount - 1)) / densestCount;
    int bw2 = (sharedTotalW - gapX * (smallestCount - 1)) / smallestCount;
    if (bw1 < 18 || bw2 < 18) {
        gapX = 2;
        bw1 = (sharedTotalW - gapX * (densestCount - 1)) / densestCount;
        bw2 = (sharedTotalW - gapX * (smallestCount - 1)) / smallestCount;
        if (bw1 < 12) bw1 = 12;
        if (bw2 < 12) bw2 = 12;
    }

    int usedRow1 = bw1 * densestCount + gapX * (densestCount - 1);
    int usedRow2 = bw2 * smallestCount + gapX * (smallestCount - 1);

    int buttonColX = rowX + titleColW + titleGap;
    int start1 = buttonColX + (buttonColW - usedRow1) / 2;
    int start2 = buttonColX + (buttonColW - usedRow2) / 2;
    if (start1 < buttonColX) start1 = buttonColX;
    if (start2 < buttonColX) start2 = buttonColX;

    int y1 = topY;
    int y2 = y1 + buttonH + rowGap;
    int y3 = y2 + buttonH + rowGap;

    *outTitle1X = rowX;
    *outTitle2X = rowX;
    *outTitle1Y = y1 + 2;
    *outTitle2Y = y2 + 2;
    *outAll = (SDL_Rect){ start1, y1, bw1, buttonH };
    *outErrors = (SDL_Rect){ start1 + bw1 + gapX, y1, bw1, buttonH };
    *outWarnings = (SDL_Rect){ start1 + (bw1 + gapX) * 2, y1, bw1, buttonH };
    *outOpenAll = (SDL_Rect){ start2, y2, bw2, buttonH };
    *outCloseAll = (SDL_Rect){ start2 + bw2 + gapX, y2, bw2, buttonH };
    if (outSearch) {
        ToolPanelControlRow searchRow =
            tool_panel_control_row_with(pane, y3, padX, d.pad_right, buttonH, gapX);
        *outSearch = ui_panel_text_field_button_strip_layout(searchRow.x_left + titleColW + titleGap,
                                                             searchRow.y,
                                                             buttonColW,
                                                             0,
                                                             20,
                                                             gapX,
                                                             searchRow.h);
    }
}

void renderErrorsPanel(UIPane* pane) {
    static bool scrollInit = false;
    if (!scrollInit) {
        scroll_state_init(errors_get_scroll_state(), NULL);
        scrollInit = true;
    }

    int contentTop = 0;
    int headerHeight = 0;
    int diagHeight = 0;
    int lineHeight = 0;
    errors_get_layout_metrics(pane, &contentTop, &headerHeight, &diagHeight, &lineHeight);
    const int contentInset = tool_panel_content_inset_default();
    const int firstRowY = contentTop + contentInset;
    TTF_Font* font = get_error_font();
    int paddingX = 12;
    int x = pane->x + paddingX;
    int viewportH = pane->h - (contentTop - pane->y);
    if (viewportH < 0) viewportH = 0;
    PaneScrollState* scroll = errors_get_scroll_state();

    int title1X = 0, title2X = 0, title1Y = 0, title2Y = 0;
    SDL_Rect btnAll = {0}, btnErrors = {0}, btnWarnings = {0}, btnOpenAll = {0}, btnCloseAll = {0};
    UIPanelTextFieldButtonStripLayout searchLayout = {0};
    layout_errors_controls(pane,
                           &title1X,
                           &title2X,
                           &title1Y,
                           &title2Y,
                           &btnAll,
                           &btnErrors,
                           &btnWarnings,
                           &btnOpenAll,
                           &btnCloseAll,
                           &searchLayout);
    drawTextWithTier(title1X, title1Y, "Show", CORE_FONT_TEXT_SIZE_CAPTION);
    drawTextWithTier(title2X, title2Y, "Batch", CORE_FONT_TEXT_SIZE_CAPTION);
    drawTextWithTier(title1X, searchLayout.text_field_rect.y + 2, "Find", CORE_FONT_TEXT_SIZE_CAPTION);
    UIPanelTaggedRectList* controlHits = errors_get_control_hits();
    ui_panel_tagged_rect_list_reset(controlHits);
    errors_set_search_strip_layout(searchLayout);
    int topMouseX = 0;
    int topMouseY = 0;
    Uint32 topMouseButtons = SDL_GetMouseState(&topMouseX, &topMouseY);
    bool topMousePressed = (topMouseButtons & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
    bool btnAllHovered = ui_panel_rect_contains(&btnAll, topMouseX, topMouseY);
    bool btnErrorsHovered = ui_panel_rect_contains(&btnErrors, topMouseX, topMouseY);
    bool btnWarningsHovered = ui_panel_rect_contains(&btnWarnings, topMouseX, topMouseY);
    bool btnOpenAllHovered = ui_panel_rect_contains(&btnOpenAll, topMouseX, topMouseY);
    bool btnCloseAllHovered = ui_panel_rect_contains(&btnCloseAll, topMouseX, topMouseY);
    bool clearSearchHovered = ui_panel_rect_contains(&searchLayout.trailing_button_rect, topMouseX, topMouseY);
    bool hasSearchQuery = errors_has_active_search_query();
    ui_panel_compact_button_render(getRenderContext()->renderer,
                                   &(UIPanelCompactButtonSpec){
                                       .rect = btnAll,
                                       .label = "All",
                                       .hovered = btnAllHovered,
                                       .active = errors_filter_all_enabled(),
                                       .pressed = btnAllHovered && topMousePressed,
                                       .disabled = false,
                                       .outlined = false,
                                       .use_custom_fill = false,
                                       .use_custom_outline = false,
                                       .tier = CORE_FONT_TEXT_SIZE_CAPTION
                                   });
    (void)ui_panel_tagged_rect_list_add(controlHits, ERROR_TOP_CONTROL_FILTER_ALL, btnAll);
    ui_panel_compact_button_render(getRenderContext()->renderer,
                                   &(UIPanelCompactButtonSpec){
                                       .rect = btnErrors,
                                       .label = "Errors",
                                       .hovered = btnErrorsHovered,
                                       .active = errors_filter_errors_enabled(),
                                       .pressed = btnErrorsHovered && topMousePressed,
                                       .disabled = false,
                                       .outlined = false,
                                       .use_custom_fill = false,
                                       .use_custom_outline = false,
                                       .tier = CORE_FONT_TEXT_SIZE_CAPTION
                                   });
    (void)ui_panel_tagged_rect_list_add(controlHits, ERROR_TOP_CONTROL_FILTER_ERRORS, btnErrors);
    ui_panel_compact_button_render(getRenderContext()->renderer,
                                   &(UIPanelCompactButtonSpec){
                                       .rect = btnWarnings,
                                       .label = "Warnings",
                                       .hovered = btnWarningsHovered,
                                       .active = errors_filter_warnings_enabled(),
                                       .pressed = btnWarningsHovered && topMousePressed,
                                       .disabled = false,
                                       .outlined = false,
                                       .use_custom_fill = false,
                                       .use_custom_outline = false,
                                       .tier = CORE_FONT_TEXT_SIZE_CAPTION
                                   });
    (void)ui_panel_tagged_rect_list_add(controlHits, ERROR_TOP_CONTROL_FILTER_WARNINGS, btnWarnings);
    ui_panel_compact_button_render(getRenderContext()->renderer,
                                   &(UIPanelCompactButtonSpec){
                                       .rect = btnOpenAll,
                                       .label = "Open All",
                                       .hovered = btnOpenAllHovered,
                                       .active = false,
                                       .pressed = btnOpenAllHovered && topMousePressed,
                                       .disabled = false,
                                       .outlined = false,
                                       .use_custom_fill = false,
                                       .use_custom_outline = false,
                                       .tier = CORE_FONT_TEXT_SIZE_CAPTION
                                   });
    (void)ui_panel_tagged_rect_list_add(controlHits, ERROR_TOP_CONTROL_OPEN_ALL, btnOpenAll);
    ui_panel_compact_button_render(getRenderContext()->renderer,
                                   &(UIPanelCompactButtonSpec){
                                       .rect = btnCloseAll,
                                       .label = "Close All",
                                       .hovered = btnCloseAllHovered,
                                       .active = false,
                                       .pressed = btnCloseAllHovered && topMousePressed,
                                       .disabled = false,
                                       .outlined = false,
                                       .use_custom_fill = false,
                                       .use_custom_outline = false,
                                       .tier = CORE_FONT_TEXT_SIZE_CAPTION
                                   });
    (void)ui_panel_tagged_rect_list_add(controlHits, ERROR_TOP_CONTROL_CLOSE_ALL, btnCloseAll);
    ui_panel_text_field_render(getRenderContext()->renderer,
                               &(UIPanelTextFieldSpec){
                                   .rect = searchLayout.text_field_rect,
                                   .text = errors_get_search_query(),
                                   .placeholder = "message, file, code...",
                                   .focused = errors_is_search_focused(),
                                   .cursor = errors_get_search_cursor(),
                                   .tier = CORE_FONT_TEXT_SIZE_CAPTION
                               });
    ui_panel_compact_button_render(getRenderContext()->renderer,
                                   &(UIPanelCompactButtonSpec){
                                       .rect = searchLayout.trailing_button_rect,
                                       .label = "x",
                                       .hovered = clearSearchHovered,
                                       .active = false,
                                       .pressed = clearSearchHovered && topMousePressed,
                                       .disabled = !hasSearchQuery,
                                       .outlined = false,
                                       .use_custom_fill = false,
                                       .use_custom_outline = false,
                                       .tier = CORE_FONT_TEXT_SIZE_CAPTION
                                   });
    (void)ui_panel_tagged_rect_list_add(controlHits,
                                        ERROR_TOP_CONTROL_CLEAR_SEARCH,
                                        searchLayout.trailing_button_rect);

    tool_panel_render_split_background(getRenderContext()->renderer, pane, contentTop, 14);

    AnalysisRefreshViewSnapshot refreshView = {0};
    analysis_refresh_view_capture(&refreshView);
    char statusBuf[128] = {0};
    analysis_refresh_view_format_status_text(&refreshView, statusBuf, sizeof(statusBuf));
    if (statusBuf[0]) {
        int tw = getTextWidth(statusBuf);
        int tx = pane->x + pane->w - tw - 16;
        int ty = tool_panel_info_line_y(pane, 0);
        drawTextWithFont(tx, ty, statusBuf, font ? font : getActiveFont());
    }

    errors_refresh_snapshot();
    FlatDiagRef refs[512];
    int flatCount = flatten_diagnostics(refs, 512);
    const Diagnostic* selectedDiag = errors_get_selected_diagnostic_ref();
    SDL_Rect detailRect = {0};
    int detailReserve = 0;
    if (selectedDiag && errors_get_detail_panel_rect(pane, &detailRect)) {
        detailReserve = detailRect.h + 12;
        viewportH -= detailReserve;
        if (viewportH < 0) viewportH = 0;
    }
    scroll_state_set_viewport(scroll, (float)viewportH);
    if (flatCount == 0) {
        SDL_Rect emptyClip = { pane->x, contentTop, pane->w - 8, viewportH };
        SDL_Color textColor = {230, 230, 230, 255};
        drawTextUTF8WithFontColorClipped(x,
                                         firstRowY,
                                         errors_has_active_search_query()
                                             ? "(No matching diagnostics)"
                                             : "(No errors or warnings)",
                                         font ? font : getActiveFont(),
                                         textColor,
                                         false,
                                         &emptyClip);
        return;
    }

    float contentHeight = (float)contentInset;
    for (int i = 0; i < flatCount; ++i) {
        contentHeight += refs[i].isHeader ? (float)headerHeight : (float)diagHeight;
    }
    scroll_state_set_content_height(scroll,
                                    scroll_state_top_anchor_content_height(scroll, contentHeight));
    float offset = scroll_state_get_offset(scroll);

    SDL_Rect clip = { pane->x, contentTop, pane->w - 8, viewportH };
    pushClipRect(&clip);

    int y = firstRowY - (int)offset;
    int maxY = contentTop + viewportH;
    SDL_Color textColor = {230, 230, 230, 255};
    int mouseX = 0;
    int mouseY = 0;
    SDL_GetMouseState(&mouseX, &mouseY);

    for (int i = 0; i < flatCount; ++i) {
        int entryHeight = refs[i].isHeader ? headerHeight : diagHeight;
        if (y + entryHeight <= contentTop) { y += entryHeight; continue; }
        if (y >= maxY) break;

        bool sel = is_error_selected(i);
        SDL_Rect highlight = { x - 8, y - 2, clip.w - paddingX + 8, entryHeight };
        UIRowSurfaceLayout highlightSurface = ui_row_surface_layout_from_rect(highlight);
        UIRowSurfaceLayout visibleSurface = {0};
        if (ui_row_surface_clip(&highlightSurface, &clip, &visibleSurface)) {
            ui_row_surface_render(getRenderContext()->renderer,
                                  &visibleSurface,
                                  &(UIRowSurfaceRenderSpec){
                                      .draw_selection_fill = sel,
                                      .draw_selection_outline = sel,
                                      .draw_hover_outline = ui_row_surface_contains(&visibleSurface, mouseX, mouseY)
                                  });
        }

        if (refs[i].isHeader) {
            char pathBuf[1024];
            const char* displayPath = error_display_path(refs[i].path, pathBuf, sizeof(pathBuf));
            drawTextUTF8WithFontColorClipped(x,
                                             y,
                                             displayPath,
                                             font ? font : getActiveFont(),
                                             textColor,
                                             false,
                                             &clip);
            y += entryHeight;
        } else {
            const Diagnostic* diag = refs[i].diag;
            const char* sev = (diag->severity == DIAG_SEVERITY_ERROR)
                ? "[E]" : (diag->severity == DIAG_SEVERITY_WARNING) ? "[W]" : "[I]";
            const char* category = diagnostic_category_name(diag->category);
            const char* codeName = (diag->codeName && diag->codeName[0])
                ? diag->codeName
                : diagnostic_code_name(diag->codeId);
            const char* stage = (diag->stage && diag->stage[0])
                ? diag->stage
                : diagnostic_stage_name(diag->codeId);
            char line[1024];
            int labelX = x + 12;
            int msgX   = x + 28;

            snprintf(line, sizeof(line), "%s %d:%d", sev, diag->line, diag->column);
            drawTextUTF8WithFontColorClipped(labelX,
                                             y,
                                             line,
                                             font ? font : getActiveFont(),
                                             textColor,
                                             false,
                                             &clip);
            y += lineHeight;

            snprintf(line, sizeof(line), "%s", diag->message ? diag->message : "(no message)");
            drawTextUTF8WithFontColorClipped(msgX,
                                             y,
                                             line,
                                             font ? font : getActiveFont(),
                                             textColor,
                                             false,
                                             &clip);
            y += lineHeight;

            if (diag->hint && diag->hint[0]) {
                snprintf(line,
                         sizeof(line),
                         "%s%s%s%s%s%s%s",
                         category ? category : "unknown",
                         (codeName && codeName[0]) ? " / " : "",
                         (codeName && codeName[0]) ? codeName : "",
                         (stage && stage[0]) ? " / " : "",
                         (stage && stage[0]) ? stage : "",
                         " / hint: ",
                         diag->hint);
            } else {
                snprintf(line,
                         sizeof(line),
                         "%s%s%s%s%s",
                         category ? category : "unknown",
                         (codeName && codeName[0]) ? " / " : "",
                         (codeName && codeName[0]) ? codeName : "",
                         (stage && stage[0]) ? " / " : "",
                         (stage && stage[0]) ? stage : "");
            }
            SDL_Color metaColor = {170, 190, 205, 255};
            drawTextUTF8WithFontColorClipped(msgX,
                                             y,
                                             line,
                                             font ? font : getActiveFont(),
                                             metaColor,
                                             false,
                                             &clip);
            y += lineHeight;
        }
    }

    popClipRect();

    bool showScrollbar = scroll_state_can_scroll(scroll) && viewportH > 0;
    if (showScrollbar) {
        SDL_Rect track = (SDL_Rect){
            pane->x + pane->w - 8,
            contentTop,
            4,
            viewportH
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
        errors_set_scroll_rects(track, thumb);
    } else {
        errors_set_scroll_rects((SDL_Rect){0}, (SDL_Rect){0});
    }

    if (selectedDiag) {
        render_selected_diagnostic_detail(pane, selectedDiag, font, lineHeight);
    }
}
