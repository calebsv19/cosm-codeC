#include "ide/Panes/ControlPanel/render_control_panel.h"
#include "engine/Render/render_pipeline.h"  // renderUIPane, drawText
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_text_helpers.h"

#include "../ControlPanel/control_panel.h"

#include "app/GlobalInfo/system_control.h"
#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/project.h"

#include "ide/Panes/PaneInfo/pane.h"
#include "ide/Panes/Editor/editor_view.h"
#include "ide/UI/Trees/tree_renderer.h"
#include "ide/UI/shared_theme_font_adapter.h"
#include "core/Analysis/analysis_status.h"
#include "core/Analysis/analysis_scheduler.h"

#include <SDL2/SDL.h>
#include <string.h>

static Uint8 clamp_u8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (Uint8)v;
}

static SDL_Color darken_color(SDL_Color c, int amount) {
    return (SDL_Color){
        clamp_u8((int)c.r - amount),
        clamp_u8((int)c.g - amount),
        clamp_u8((int)c.b - amount),
        c.a
    };
}

static bool same_rgb(SDL_Color a, SDL_Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

typedef struct {
    ControlFilterButtonId id;
    const char* label;
} FilterButtonSpec;

static int render_filter_group(SDL_Renderer* renderer,
                               int rowX,
                               int y,
                               int titleColW,
                               int buttonColW,
                               const char* title,
                               const FilterButtonSpec* specs,
                               int specCount,
                               int buttonW,
                               int gapX) {
    if (!renderer || !title || !specs || specCount <= 0) return y;

    SDL_Color fill = {80, 80, 80, 255};
    SDL_Color fillActive = {120, 120, 120, 255};
    SDL_Color border = {180, 180, 180, 255};
    SDL_Color text = {230, 230, 230, 255};
    ide_shared_theme_button_colors(&fill, &fillActive, &border, &text);

    const int buttonH = 16;
    const int titleGap = 7;
    const int titleW = getTextWidth(title);
    int titleX = rowX;
    int titleY = y + 2;
    drawTextWithTier(titleX, titleY, title, CORE_FONT_TEXT_SIZE_CAPTION);

    int rowStartX = rowX + titleColW + titleGap;
    int available = buttonColW;
    if (available < 1) available = 1;
    int bw = buttonW;
    if (bw < 18) bw = 18;
    if (gapX < 2) gapX = 2;
    int startX = rowStartX;
    (void)titleW;
    if (startX < rowStartX) startX = rowStartX;

    int cx = startX;
    for (int i = 0; i < specCount; ++i) {
        const char* label = specs[i].label ? specs[i].label : "";
        SDL_Rect rect = { cx, y, bw, buttonH };
        bool active = control_panel_is_filter_button_active(specs[i].id);

        SDL_Color bg = active ? fillActive : fill;
        SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 255);
        SDL_RenderFillRect(renderer, &rect);
        SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, 255);
        SDL_RenderDrawRect(renderer, &rect);
        if (control_panel_is_match_button_selected(specs[i].id)) {
            SDL_Rect highlight = {
                rect.x - 1,
                rect.y - 1,
                rect.w + 2,
                rect.h + 2
            };
            SDL_SetRenderDrawColor(renderer, 232, 214, 162, 255);
            SDL_RenderDrawRect(renderer, &highlight);
        }

        int textMaxW = rect.w - 8;
        size_t keepLen = getTextClampedLength(label, textMaxW);
        char labelBuf[64];
        if (keepLen >= sizeof(labelBuf)) keepLen = sizeof(labelBuf) - 1;
        memcpy(labelBuf, label, keepLen);
        labelBuf[keepLen] = '\0';

        int tx = rect.x + (rect.w - getTextWidth(labelBuf)) / 2;
        int ty = rect.y + 2;
        drawTextWithTier(tx, ty, labelBuf, CORE_FONT_TEXT_SIZE_CAPTION);

        control_panel_register_filter_button(specs[i].id, rect);
        cx += bw + gapX;
    }

    return y + buttonH + 5;
}

void renderControlPanelContents(UIPane* pane, bool hovered, struct IDECoreState* core) {
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer* renderer = ctx->renderer;

    renderUIPane(pane, hovered);

    int padding = 8;
    int x = pane->x + padding;
    int y = pane->y + padding;

    // Panel title
    drawTextWithTier(x, y, pane->title, CORE_FONT_TEXT_SIZE_HEADER);
    AnalysisStatusSnapshot snap = {0};
    AnalysisSchedulerSnapshot sched = {0};
    analysis_status_snapshot(&snap);
    analysis_scheduler_snapshot(&sched);
    if (snap.updating || snap.last_error[0] || snap.has_cache) {
        char statusBuf[128] = {0};
        if (snap.updating) {
            snprintf(statusBuf, sizeof(statusBuf),
                     sched.active_run_id ? "Updating (#%llu)..." : "Updating...",
                     (unsigned long long)sched.active_run_id);
        } else if (snap.last_error[0]) {
            snprintf(statusBuf, sizeof(statusBuf), "Analysis error");
        } else if (snap.status == ANALYSIS_STATUS_FRESH) {
            snprintf(statusBuf, sizeof(statusBuf), "Loaded");
        } else if (snap.has_cache) {
            snprintf(statusBuf, sizeof(statusBuf), "(cached)");
        }
        if (statusBuf[0]) {
            int tw = getTextWidth(statusBuf);
            int tx = pane->x + pane->w - tw - 16;
            int ty = pane->y + 8;
            drawTextWithTier(tx, ty, statusBuf, CORE_FONT_TEXT_SIZE_CAPTION);
        }
    }
    if (!snap.updating && !snap.last_error[0]) {
        char runBuf[128] = {0};
        if (snap.refresh_mode == ANALYSIS_REFRESH_MODE_INCREMENTAL) {
            snprintf(runBuf,
                     sizeof(runBuf),
                     "Inc d:%d r:%d dep:%d t:%d",
                     snap.dirty_files,
                     snap.removed_files,
                     snap.dependent_files,
                     snap.target_files);
        } else if (snap.refresh_mode == ANALYSIS_REFRESH_MODE_FULL) {
            snprintf(runBuf, sizeof(runBuf), "Full rebuild");
        }
        if (runBuf[0]) {
            drawTextWithTier(x, y + 13, runBuf, CORE_FONT_TEXT_SIZE_CAPTION);
        }
    }
    y += 34;

    // Search bar label
    drawTextWithTier(x, y, "Search", CORE_FONT_TEXT_SIZE_CAPTION);
    y += 14;

    const int pauseW = 20;
    const int clearW = 20;
    const int gap = 6;
    int searchW = pane->w - 2 * padding - pauseW - clearW - (2 * gap);
    if (searchW < 40) searchW = 40;
    SDL_Rect searchBox = { x, y, searchW, 22 };
    SDL_Rect pauseBtn = { x + searchW + gap, y, pauseW, 22 };
    SDL_Rect clearBtn = { pauseBtn.x + pauseW + gap, y, clearW, 22 };
    control_panel_set_search_box_rect(searchBox);
    control_panel_set_search_pause_button_rect(pauseBtn);
    control_panel_set_search_clear_button_rect(clearBtn);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 20);  // faint fill
    SDL_RenderFillRect(renderer, &searchBox);
    if (control_panel_is_search_focused()) {
        SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255);
    } else {
        SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
    }
    SDL_RenderDrawRect(renderer, &searchBox);
    const char* query = control_panel_get_search_query();
    if (query && query[0]) {
        drawTextWithTier(x + 6, y + 3, query, CORE_FONT_TEXT_SIZE_CAPTION);
    } else if (!control_panel_is_search_focused()) {
        drawTextWithTier(x + 6, y + 3, "type to filter...", CORE_FONT_TEXT_SIZE_CAPTION);
    }

    if (control_panel_is_search_focused()) {
        int cursor = control_panel_get_search_cursor();
        if (cursor < 0) cursor = 0;
        size_t queryLen = query ? strlen(query) : 0;
        if ((size_t)cursor > queryLen) cursor = (int)queryLen;

        char beforeCursor[256];
        size_t prefixLen = (size_t)cursor;
        if (prefixLen >= sizeof(beforeCursor)) prefixLen = sizeof(beforeCursor) - 1;
        if (query && prefixLen > 0) {
            memcpy(beforeCursor, query, prefixLen);
        }
        beforeCursor[prefixLen] = '\0';

        int caretX = x + 6 + getTextWidth(beforeCursor);
        SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
        SDL_RenderDrawLine(renderer, caretX, y + 3, caretX, y + 18);
    }

    bool searchEnabled = control_panel_is_search_enabled();
    SDL_Color pauseFill = searchEnabled ? (SDL_Color){255, 255, 255, 20} : (SDL_Color){95, 95, 95, 160};
    SDL_SetRenderDrawColor(renderer, pauseFill.r, pauseFill.g, pauseFill.b, pauseFill.a);
    SDL_RenderFillRect(renderer, &pauseBtn);
    SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
    SDL_RenderDrawRect(renderer, &pauseBtn);
    drawTextWithTier(pauseBtn.x + 6, pauseBtn.y + 3, "||", CORE_FONT_TEXT_SIZE_CAPTION);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 20);
    SDL_RenderFillRect(renderer, &clearBtn);
    SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
    SDL_RenderDrawRect(renderer, &clearBtn);
    drawTextWithTier(clearBtn.x + 6, clearBtn.y + 3, "x", CORE_FONT_TEXT_SIZE_CAPTION);
    y += 30;

    const bool collapsed = control_panel_filters_collapsed();
    char filterTitle[32];
    snprintf(filterTitle, sizeof(filterTitle), "%s Filters", collapsed ? "[+]" : "[-]");
    drawTextWithTier(x, y, filterTitle, CORE_FONT_TEXT_SIZE_CAPTION);
    int titleW = getTextWidth(filterTitle);
    SDL_Rect filterHeader = { x - 2, y - 1, titleW + 8, 14 };
    control_panel_set_filter_header_rect(filterHeader);
    y += 12;

    control_panel_begin_filter_button_frame();

    const int filterW = pane->w - (2 * padding);
    const FilterButtonSpec targetButtons[] = {
        { CONTROL_FILTER_BTN_TARGET_SYMBOLS, "Symbols" },
        { CONTROL_FILTER_BTN_TARGET_EDITOR, "Editor" }
    };
    const FilterButtonSpec scopeButtons[] = {
        { CONTROL_FILTER_BTN_SCOPE_ACTIVE, "Active" },
        { CONTROL_FILTER_BTN_SCOPE_PROJECT, "Project" }
    };
    FilterButtonSpec matchButtons[5];
    matchButtons[0] = (FilterButtonSpec){ CONTROL_FILTER_BTN_MATCH_ALL, "All" };
    ControlFilterButtonId matchOrder[4] = {0};
    control_panel_get_match_button_order(matchOrder);
    for (int i = 0; i < 4; ++i) {
        const char* label = "Tag";
        switch (matchOrder[i]) {
            case CONTROL_FILTER_BTN_MATCH_METHODS: label = "Methods"; break;
            case CONTROL_FILTER_BTN_MATCH_TYPES: label = "Types"; break;
            case CONTROL_FILTER_BTN_MATCH_VARS: label = "Vars"; break;
            case CONTROL_FILTER_BTN_MATCH_TAGS:
            default:
                label = "Tags";
                break;
        }
        matchButtons[i + 1].id = matchOrder[i];
        matchButtons[i + 1].label = label;
    }
    const FilterButtonSpec editorViewButtons[] = {
        { CONTROL_FILTER_BTN_EDITOR_VIEW_PROJECTION, "Projection" },
        { CONTROL_FILTER_BTN_EDITOR_VIEW_MARKERS, "Markers" }
    };
    const FilterButtonSpec parseButtons[] = {
        { CONTROL_FILTER_BTN_LIVE_PARSE, "Live" },
        { CONTROL_FILTER_BTN_INLINE_ERRORS, "Inline Err" },
        { CONTROL_FILTER_BTN_MACROS, "Macros" }
    };

    int gapX = 5;
    int targetCount = (int)(sizeof(targetButtons) / sizeof(targetButtons[0]));
    int scopeCount = (int)(sizeof(scopeButtons) / sizeof(scopeButtons[0]));
    int matchCount = (int)(sizeof(matchButtons) / sizeof(matchButtons[0]));
    int editorViewCount = (int)(sizeof(editorViewButtons) / sizeof(editorViewButtons[0]));
    int parseCount = (int)(sizeof(parseButtons) / sizeof(parseButtons[0]));
    const bool editorTargetEnabled = control_panel_target_editor_enabled();
    const bool showEditorViewRow = editorTargetEnabled;

    const FilterButtonSpec* groups[5];
    const char* groupTitles[5];
    int groupCounts[5];
    int groupTotal = 0;
    int matchGroupIndex = -1;

    groups[groupTotal] = targetButtons;
    groupTitles[groupTotal] = "Target";
    groupCounts[groupTotal] = targetCount;
    groupTotal++;

    groups[groupTotal] = scopeButtons;
    groupTitles[groupTotal] = "Scope";
    groupCounts[groupTotal] = scopeCount;
    groupTotal++;

    groups[groupTotal] = matchButtons;
    groupTitles[groupTotal] = "Match";
    groupCounts[groupTotal] = matchCount;
    matchGroupIndex = groupTotal;
    groupTotal++;

    if (showEditorViewRow) {
        groups[groupTotal] = editorViewButtons;
        groupTitles[groupTotal] = "Editor";
        groupCounts[groupTotal] = editorViewCount;
        groupTotal++;
    }

    groups[groupTotal] = parseButtons;
    groupTitles[groupTotal] = "Parse";
    groupCounts[groupTotal] = parseCount;
    groupTotal++;

    if (!collapsed) {
        int titleColW = 0;
        const int titleGap = 7;
        for (int gi = 0; gi < groupTotal; ++gi) {
            int tw = getTextWidth(groupTitles[gi]);
            if (tw > titleColW) titleColW = tw;
        }
        if (titleColW < 32) titleColW = 32;

        int buttonColumnAvailable = filterW - titleColW - titleGap;
        if (buttonColumnAvailable < 1) buttonColumnAvailable = 1;

        int matchLongestLabelW = 0;
        for (int gi = 0; gi < groupTotal; ++gi) {
            for (int bi = 0; bi < groupCounts[gi]; ++bi) {
                const char* label = groups[gi][bi].label ? groups[gi][bi].label : "";
                int lw = getTextWidth(label);
                if (gi == matchGroupIndex && lw > matchLongestLabelW) {
                    matchLongestLabelW = lw;
                }
            }
        }

        // Freeze based on Match row reaching full label width.
        int matchButtonMaxW = matchLongestLabelW + 24;
        if (matchButtonMaxW < 34) matchButtonMaxW = 34;
        int matchButtons = (matchGroupIndex >= 0) ? groupCounts[matchGroupIndex] : 1;
        if (matchButtons < 1) matchButtons = 1;
        int freezeButtonsW = matchButtonMaxW * matchButtons + gapX * (matchButtons - 1);
        if (freezeButtonsW < 1) freezeButtonsW = 1;

        int effectiveButtonsW = buttonColumnAvailable;
        if (effectiveButtonsW > freezeButtonsW) effectiveButtonsW = freezeButtonsW;
        bool freezeActive = (buttonColumnAvailable > freezeButtonsW);

        int rowButtonW[5] = {0};
        for (int gi = 0; gi < groupTotal; ++gi) {
            int count = groupCounts[gi];
            if (count < 1) count = 1;
            int bw = (effectiveButtonsW - gapX * (count - 1)) / count;
            if (bw < 18) bw = 18;
            rowButtonW[gi] = bw;
        }

        // Safety fallback for very narrow panels.
        int densestNeeded = rowButtonW[0] * groupCounts[0] + gapX * (groupCounts[0] - 1);
        for (int gi = 1; gi < groupTotal; ++gi) {
            int needed = rowButtonW[gi] * groupCounts[gi] + gapX * (groupCounts[gi] - 1);
            if (needed > densestNeeded) densestNeeded = needed;
        }
        if (densestNeeded > filterW && gapX > 2) {
            gapX = 2;
            for (int gi = 0; gi < groupTotal; ++gi) {
                int count = groupCounts[gi];
                int bw = (effectiveButtonsW - gapX * (count - 1)) / count;
                if (bw < 18) bw = 18;
                rowButtonW[gi] = bw;
            }
        }

        int blockW = titleColW + titleGap + effectiveButtonsW;
        int rowX = x;
        if (freezeActive && blockW < filterW) {
            rowX = x + (filterW - blockW) / 2;
        }

        for (int gi = 0; gi < groupTotal; ++gi) {
            y = render_filter_group(renderer,
                                    rowX,
                                    y,
                                    titleColW,
                                    effectiveButtonsW,
                                    groupTitles[gi],
                                    groups[gi],
                                    groupCounts[gi],
                                    rowButtonW[gi],
                                    gapX);
        }
    } else {
        y += 2;
    }

    int listTop = y + 10;
    control_panel_set_symbol_list_top(listTop);
    int listHeight = (pane->y + pane->h) - listTop;
    if (listHeight < 0) listHeight = 0;

    SDL_Color editorBg = ide_shared_theme_background_color();
    SDL_Color symbolsBgColor = editorBg;
    if (same_rgb(symbolsBgColor, pane->bgColor)) {
        // Guarantee visual separation even when theme adapter returns same tone.
        symbolsBgColor = darken_color(pane->bgColor, 14);
    }
    SDL_Rect symbolsBg = {
        pane->x + 1,
        listTop,
        pane->w - 2,
        listHeight
    };
    SDL_SetRenderDrawColor(renderer, symbolsBgColor.r, symbolsBgColor.g, symbolsBgColor.b, 255);
    SDL_RenderFillRect(renderer, &symbolsBg);

    OpenFile* activeFile = NULL;
    if (core && core->activeEditorView) {
        activeFile = getActiveOpenFile(core->activeEditorView);
    }
    const char* activePath = activeFile ? activeFile->filePath : NULL;
    control_panel_refresh_symbol_tree(projectRoot, activePath);

    UITreeNode* tree = control_panel_get_symbol_tree();
    PaneScrollState* scroll = control_panel_get_symbol_scroll();
    SDL_Rect* track = control_panel_get_symbol_scroll_track();
    SDL_Rect* thumb = control_panel_get_symbol_scroll_thumb();

    UIPane listPane = *pane;
    listPane.y = control_panel_get_symbol_tree_origin_y(pane);
    listPane.h = (pane->y + pane->h) - listPane.y;
    if (listPane.h < 0) listPane.h = 0;

    if (tree) {
        renderTreePanelWithScroll(&listPane, tree, scroll, track, thumb);
    }
}
