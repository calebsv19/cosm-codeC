#include "ide/Panes/ToolPanels/Errors/render_tool_errors.h"
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_font.h"

#include "ide/Panes/ToolPanels/Errors/tool_errors.h"
#include "ide/Panes/ToolPanels/tool_panel_chrome.h"
#include "ide/Panes/ToolPanels/tool_panel_top_layout.h"
#include "core/Diagnostics/diagnostics_engine.h"
#include "core/Analysis/analysis_status.h"
#include "core/Analysis/analysis_scheduler.h"
#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/project.h"
#include "engine/Render/render_pipeline.h"
#include "ide/UI/scroll_manager.h"
#include "engine/Render/render_text_helpers.h"
#include "ide/UI/shared_theme_font_adapter.h"
#include "ide/UI/ui_selection_style.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

// Forward from tool_errors.c
bool is_error_selected(int idx);
TTF_Font* get_error_font(void);
void errors_get_layout_metrics(const UIPane* pane, int* contentTop, int* headerHeight, int* diagHeight, int* lineHeight);
static SDL_Rect gErrorScrollTrack = {0};
static SDL_Rect gErrorScrollThumb = {0};

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

static void render_state_button(const UIPane* pane,
                                SDL_Rect rect,
                                const char* label,
                                bool active) {
    (void)pane;
    SDL_Renderer* r = getRenderContext()->renderer;
    SDL_Color fill = {80, 80, 80, 255};
    SDL_Color fillActive = {120, 120, 120, 255};
    SDL_Color border = {180, 180, 180, 255};
    SDL_Color text = {230, 230, 230, 255};
    ide_shared_theme_button_colors(&fill, &fillActive, &border, &text);

    SDL_Color bg = active ? fillActive : fill;
    SDL_SetRenderDrawColor(r, bg.r, bg.g, bg.b, 255);
    SDL_RenderFillRect(r, &rect);
    SDL_SetRenderDrawColor(r, border.r, border.g, border.b, 255);
    SDL_RenderDrawRect(r, &rect);

    const char* src = label ? label : "";
    int textMaxW = rect.w - 8;
    size_t keepLen = getTextClampedLength(src, textMaxW);
    char labelBuf[64];
    if (keepLen >= sizeof(labelBuf)) keepLen = sizeof(labelBuf) - 1;
    memcpy(labelBuf, src, keepLen);
    labelBuf[keepLen] = '\0';

    int tx = rect.x + (rect.w - getTextWidth(labelBuf)) / 2;
    int ty = rect.y + 3;
    drawTextWithTier(tx, ty, labelBuf, CORE_FONT_TEXT_SIZE_CAPTION);
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

static void layout_errors_controls(const UIPane* pane,
                                   int* outTitle1X,
                                   int* outTitle2X,
                                   int* outTitle1Y,
                                   int* outTitle2Y,
                                   SDL_Rect* outAll,
                                   SDL_Rect* outErrors,
                                   SDL_Rect* outWarnings,
                                   SDL_Rect* outOpenAll,
                                   SDL_Rect* outCloseAll) {
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

    *outTitle1X = rowX;
    *outTitle2X = rowX;
    *outTitle1Y = y1 + 2;
    *outTitle2Y = y2 + 2;
    *outAll = (SDL_Rect){ start1, y1, bw1, buttonH };
    *outErrors = (SDL_Rect){ start1 + bw1 + gapX, y1, bw1, buttonH };
    *outWarnings = (SDL_Rect){ start1 + (bw1 + gapX) * 2, y1, bw1, buttonH };
    *outOpenAll = (SDL_Rect){ start2, y2, bw2, buttonH };
    *outCloseAll = (SDL_Rect){ start2 + bw2 + gapX, y2, bw2, buttonH };
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
    scroll_state_set_viewport(scroll, (float)viewportH);

    int title1X = 0, title2X = 0, title1Y = 0, title2Y = 0;
    SDL_Rect btnAll = {0}, btnErrors = {0}, btnWarnings = {0}, btnOpenAll = {0}, btnCloseAll = {0};
    layout_errors_controls(pane,
                           &title1X,
                           &title2X,
                           &title1Y,
                           &title2Y,
                           &btnAll,
                           &btnErrors,
                           &btnWarnings,
                           &btnOpenAll,
                           &btnCloseAll);
    drawTextWithTier(title1X, title1Y, "Show", CORE_FONT_TEXT_SIZE_CAPTION);
    drawTextWithTier(title2X, title2Y, "Batch", CORE_FONT_TEXT_SIZE_CAPTION);
    errors_set_control_button_rects(btnAll, btnErrors, btnWarnings, btnOpenAll, btnCloseAll);
    render_state_button(pane, btnAll, "All", errors_filter_all_enabled());
    render_state_button(pane, btnErrors, "Errors", errors_filter_errors_enabled());
    render_state_button(pane, btnWarnings, "Warnings", errors_filter_warnings_enabled());
    renderButton(pane, btnOpenAll, "Open All");
    renderButton(pane, btnCloseAll, "Close All");

    tool_panel_render_split_background(getRenderContext()->renderer, pane, contentTop, 14);

    AnalysisStatusSnapshot snap = {0};
    AnalysisSchedulerSnapshot sched = {0};
    int progressCompleted = 0;
    int progressTotal = 0;
    analysis_status_snapshot(&snap);
    analysis_status_get_progress(&progressCompleted, &progressTotal);
    analysis_scheduler_snapshot(&sched);
    char statusBuf[128] = {0};
    if (snap.updating) {
        if (progressTotal > 0) {
            snprintf(statusBuf, sizeof(statusBuf),
                     sched.active_run_id ? "Updating %d/%d (#%llu)" : "Updating %d/%d",
                     progressCompleted,
                     progressTotal,
                     (unsigned long long)sched.active_run_id);
        } else {
            snprintf(statusBuf, sizeof(statusBuf),
                     sched.active_run_id ? "Updating (#%llu)..." : "Updating...",
                     (unsigned long long)sched.active_run_id);
        }
    } else if (snap.last_error[0]) {
        snprintf(statusBuf, sizeof(statusBuf), "Analysis error");
    } else if (snap.has_cache) {
        snprintf(statusBuf, sizeof(statusBuf), "(cached)");
    }
    if (statusBuf[0]) {
        int tw = getTextWidth(statusBuf);
        int tx = pane->x + pane->w - tw - 16;
        int ty = tool_panel_info_line_y(pane, 0);
        drawTextWithFont(tx, ty, statusBuf, font ? font : getActiveFont());
    }

    errors_refresh_snapshot();
    FlatDiagRef refs[512];
    int flatCount = flatten_diagnostics(refs, 512);
    if (flatCount == 0) {
        drawTextWithFont(x, firstRowY, "(No errors or warnings)", font ? font : getActiveFont());
        return;
    }

    float contentHeight = (float)contentInset;
    for (int i = 0; i < flatCount; ++i) {
        contentHeight += refs[i].isHeader ? (float)headerHeight : (float)diagHeight;
    }
    scroll_state_set_content_height(scroll, contentHeight);
    float offset = scroll_state_get_offset(scroll);

    SDL_Rect clip = { pane->x, contentTop, pane->w - 8, viewportH };
    pushClipRect(&clip);

    int y = firstRowY - (int)offset;
    int maxY = contentTop + viewportH;

    for (int i = 0; i < flatCount; ++i) {
        int entryHeight = refs[i].isHeader ? headerHeight : diagHeight;
        if (y + entryHeight <= contentTop) { y += entryHeight; continue; }
        if (y >= maxY) break;

        bool sel = is_error_selected(i);
        if (sel) {
            SDL_Rect highlight = { x - 8, y - 2, clip.w - paddingX + 8, entryHeight };
            SDL_Color selColor = ui_selection_fill_color();
            SDL_SetRenderDrawColor(getRenderContext()->renderer, selColor.r, selColor.g, selColor.b, selColor.a);
            SDL_RenderFillRect(getRenderContext()->renderer, &highlight);
        }

        if (refs[i].isHeader) {
            char pathBuf[1024];
            const char* displayPath = error_display_path(refs[i].path, pathBuf, sizeof(pathBuf));
            drawTextWithFont(x, y, displayPath, font ? font : getActiveFont());
            y += entryHeight;
        } else {
            const Diagnostic* diag = refs[i].diag;
            const char* sev = (diag->severity == DIAG_SEVERITY_ERROR)
                ? "[E]" : (diag->severity == DIAG_SEVERITY_WARNING) ? "[W]" : "[I]";
            char line[1024];
            int labelX = x + 12;
            int msgX   = x + 28;

            snprintf(line, sizeof(line), "%s %d:%d", sev, diag->line, diag->column);
            drawTextWithFont(labelX, y, line, font ? font : getActiveFont());
            y += lineHeight;

            snprintf(line, sizeof(line), "%s", diag->message ? diag->message : "(no message)");
            drawTextWithFont(msgX, y, line, font ? font : getActiveFont());
            y += lineHeight;
        }
    }

    popClipRect();

    bool showScrollbar = scroll_state_can_scroll(scroll) && viewportH > 0;
    if (showScrollbar) {
        gErrorScrollTrack = (SDL_Rect){
            pane->x + pane->w - 8,
            contentTop,
            4,
            viewportH
        };
        gErrorScrollThumb = scroll_state_thumb_rect(scroll,
                                                   gErrorScrollTrack.x,
                                                   gErrorScrollTrack.y,
                                                   gErrorScrollTrack.w,
                                                   gErrorScrollTrack.h);
        SDL_Color trackColor = scroll->track_color;
        SDL_Color thumbColor = scroll->thumb_color;
        SDL_SetRenderDrawColor(getRenderContext()->renderer, trackColor.r, trackColor.g, trackColor.b, trackColor.a);
        SDL_RenderFillRect(getRenderContext()->renderer, &gErrorScrollTrack);
        SDL_SetRenderDrawColor(getRenderContext()->renderer, thumbColor.r, thumbColor.g, thumbColor.b, thumbColor.a);
        SDL_RenderFillRect(getRenderContext()->renderer, &gErrorScrollThumb);
    } else {
        gErrorScrollTrack = (SDL_Rect){0};
        gErrorScrollThumb = (SDL_Rect){0};
    }

    errors_set_scroll_rects(gErrorScrollTrack, gErrorScrollThumb);
}
