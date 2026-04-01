#include "ide/Panes/ControlPanel/render_control_panel.h"
#include "engine/Render/render_pipeline.h"  // renderUIPane, drawText
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_text_helpers.h"

#include "../ControlPanel/control_panel.h"

#include "app/GlobalInfo/system_control.h"
#include "app/GlobalInfo/core_state.h"

#include "ide/Panes/PaneInfo/pane.h"
#include "ide/UI/panel_control_widgets.h"
#include "ide/UI/Trees/tree_renderer.h"
#include "ide/UI/panel_text_field.h"
#include "ide/UI/shared_theme_font_adapter.h"
#include "ide/Panes/ToolPanels/tool_panel_chrome.h"
#include "ide/Panes/ToolPanels/tool_panel_top_layout.h"
#include "core/Analysis/analysis_status.h"

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

typedef struct {
    ControlFilterButtonId id;
    const char* label;
} FilterButtonSpec;

static void control_panel_format_header_status(const AnalysisStatusSnapshot* snap,
                                               char* out,
                                               size_t out_cap) {
    if (!out || out_cap == 0) return;
    out[0] = '\0';
    if (!snap) return;

    if (snap->last_error[0]) {
        snprintf(out, out_cap, "Analysis error");
        return;
    }
    if (snap->updating) {
        snprintf(out, out_cap, "Updating");
        return;
    }
    if (snap->status == ANALYSIS_STATUS_FRESH) {
        snprintf(out, out_cap, "Loaded");
        return;
    }
    if (snap->status == ANALYSIS_STATUS_STALE_LOADING) {
        snprintf(out, out_cap, "Loading");
        return;
    }
    if (snap->has_cache) {
        snprintf(out, out_cap, "Cached");
        return;
    }
    snprintf(out, out_cap, "Idle");
}

static void control_panel_format_secondary_summary(const AnalysisStatusSnapshot* snap,
                                                   char* out,
                                                   size_t out_cap,
                                                   bool last_run_prefix) {
    if (!out || out_cap == 0) return;
    out[0] = '\0';
    if (!snap) return;

    const char* prefix = last_run_prefix ? "Last run: " : "Mode: ";
    if (snap->refresh_mode == ANALYSIS_REFRESH_MODE_INCREMENTAL) {
        snprintf(out,
                 out_cap,
                 "%sinc d:%d r:%d dep:%d t:%d",
                 prefix,
                 snap->dirty_files,
                 snap->removed_files,
                 snap->dependent_files,
                 snap->target_files);
    } else if (snap->refresh_mode == ANALYSIS_REFRESH_MODE_FULL) {
        snprintf(out, out_cap, "%sfull rebuild", prefix);
    } else if (snap->updating) {
        snprintf(out, out_cap, "%srefreshing", prefix);
    } else {
        snprintf(out, out_cap, "%snone", prefix);
    }
}

static void control_panel_format_status_lines(const AnalysisStatusSnapshot* snap,
                                              int progress_completed,
                                              int progress_total,
                                              char* line1,
                                              size_t line1_cap,
                                              char* line2,
                                              size_t line2_cap) {
    if (line1 && line1_cap > 0) line1[0] = '\0';
    if (line2 && line2_cap > 0) line2[0] = '\0';
    if (!snap || !line1 || line1_cap == 0 || !line2 || line2_cap == 0) return;

    if (snap->last_error[0]) {
        snprintf(line1, line1_cap, "Status: analysis error");
        snprintf(line2, line2_cap, "%s", snap->last_error);
        return;
    }

    if (snap->updating) {
        if (progress_total > 0) {
            snprintf(line1, line1_cap, "Status: updating %d/%d", progress_completed, progress_total);
        } else {
            snprintf(line1, line1_cap, "Status: updating");
        }
        control_panel_format_secondary_summary(snap, line2, line2_cap, false);
        return;
    }

    if (snap->status == ANALYSIS_STATUS_FRESH) {
        snprintf(line1, line1_cap, "Status: ready");
    } else if (snap->status == ANALYSIS_STATUS_STALE_LOADING) {
        snprintf(line1, line1_cap, "Status: loading");
    } else if (snap->has_cache) {
        snprintf(line1, line1_cap, "Status: cache loaded");
    } else {
        snprintf(line1, line1_cap, "Status: idle");
    }
    control_panel_format_secondary_summary(snap, line2, line2_cap, true);
}

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

    UIPanelCompactButtonRowItem rowItems[8];
    int rowItemCount = specCount;
    if (rowItemCount > (int)(sizeof(rowItems) / sizeof(rowItems[0]))) {
        rowItemCount = (int)(sizeof(rowItems) / sizeof(rowItems[0]));
    }
    for (int i = 0; i < rowItemCount; ++i) {
        rowItems[i] = (UIPanelCompactButtonRowItem){
            .tag = (int)specs[i].id,
            .label = specs[i].label ? specs[i].label : "",
            .active = control_panel_is_filter_button_active(specs[i].id),
            .outlined = control_panel_is_match_button_selected(specs[i].id)
        };
    }
    ui_panel_compact_button_row_render(renderer,
                                       startX,
                                       y,
                                       bw,
                                       buttonH,
                                       gapX,
                                       rowItems,
                                       rowItemCount,
                                       CORE_FONT_TEXT_SIZE_CAPTION,
                                       control_panel_get_filter_button_hit_list());

    return y + buttonH + 5;
}

void renderControlPanelContents(UIPane* pane, bool hovered, struct IDECoreState* core) {
    RenderContext* ctx = getRenderContext();
    IDEThemePalette palette = {0};
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer* renderer = ctx->renderer;
    ide_shared_theme_resolve_palette(&palette);
    (void)hovered;
    (void)core;

    ToolPanelLayoutDefaults d = tool_panel_layout_defaults();
    int x = pane->x + d.pad_left;
    int y = pane->y + d.controls_top - 1;
    const int searchRowHeight = d.button_h;

    // Panel title
    drawTextWithTier(x, tool_panel_info_line_y(pane, 0), pane->title, CORE_FONT_TEXT_SIZE_HEADER);
    AnalysisStatusSnapshot snap = {0};
    int progressCompleted = 0;
    int progressTotal = 0;
    analysis_status_snapshot(&snap);
    analysis_status_get_progress(&progressCompleted, &progressTotal);
    char headerStatus[64] = {0};
    control_panel_format_header_status(&snap, headerStatus, sizeof(headerStatus));
    if (headerStatus[0]) {
        int tw = getTextWidth(headerStatus);
        int tx = pane->x + pane->w - tw - 16;
        int ty = tool_panel_info_line_y(pane, 0);
        if (snap.last_error[0]) {
            drawTextWithTierError(tx, ty, headerStatus, CORE_FONT_TEXT_SIZE_CAPTION);
        } else {
            drawTextWithTierMuted(tx, ty, headerStatus, CORE_FONT_TEXT_SIZE_CAPTION);
        }
    }

    const int statusLineSlots = 2;
    int infoStartY = y + searchRowHeight + d.row_gap;
    char statusLine1[192] = {0};
    char statusLine2[256] = {0};
    control_panel_format_status_lines(&snap,
                                      progressCompleted,
                                      progressTotal,
                                      statusLine1,
                                      sizeof(statusLine1),
                                      statusLine2,
                                      sizeof(statusLine2));
    drawTextWithTierMuted(x, infoStartY, statusLine1, CORE_FONT_TEXT_SIZE_CAPTION);
    if (snap.last_error[0]) {
        drawTextWithTierError(x, infoStartY + d.info_line_gap, statusLine2, CORE_FONT_TEXT_SIZE_CAPTION);
    } else {
        drawTextWithTierMuted(x, infoStartY + d.info_line_gap, statusLine2, CORE_FONT_TEXT_SIZE_CAPTION);
    }

    const int pauseW = 20;
    const int clearW = 20;
    const int gap = 6;
    ToolPanelControlRow searchRow =
        tool_panel_control_row_with(pane, y, d.pad_left, d.pad_right, searchRowHeight, gap);
    UIPanelTextFieldButtonStripLayout searchLayout =
        ui_panel_text_field_button_strip_layout(searchRow.x_left,
                                                searchRow.y,
                                                searchRow.x_right - searchRow.x_left,
                                                pauseW,
                                                clearW,
                                                gap,
                                                searchRow.h);
    if (searchLayout.text_field_rect.w < 40) {
        searchLayout.text_field_rect.w = 40;
        searchLayout.aux_button_rect.x = searchLayout.text_field_rect.x +
                                         searchLayout.text_field_rect.w + gap;
        searchLayout.trailing_button_rect.x = searchLayout.aux_button_rect.x +
                                              searchLayout.aux_button_rect.w + gap;
    }
    SDL_Rect searchBox = searchLayout.text_field_rect;
    SDL_Rect pauseBtn = searchLayout.aux_button_rect;
    SDL_Rect clearBtn = searchLayout.trailing_button_rect;
    control_panel_set_search_strip_layout(searchLayout);
    const char* query = control_panel_get_search_query();
    ui_panel_text_field_render(renderer,
                               &(UIPanelTextFieldSpec){
                                   .rect = searchBox,
                                   .text = query,
                                   .placeholder = "type to filter...",
                                   .focused = control_panel_is_search_focused(),
                                   .cursor = control_panel_get_search_cursor(),
                                   .tier = CORE_FONT_TEXT_SIZE_CAPTION
                               });

    bool searchEnabled = control_panel_is_search_enabled();
    SDL_Color pauseFill = searchEnabled ? palette.input_fill : darken_color(palette.button_fill, 20);
    ui_panel_compact_button_render(renderer,
                                   &(UIPanelCompactButtonSpec){
                                       .rect = pauseBtn,
                                       .label = "||",
                                       .active = false,
                                       .outlined = false,
                                       .use_custom_fill = true,
                                       .custom_fill = pauseFill,
                                       .use_custom_outline = true,
                                       .custom_outline = palette.input_border,
                                       .tier = CORE_FONT_TEXT_SIZE_CAPTION
                                   });

    SDL_Color clearFill = palette.input_fill;
    clearFill.a = 80;
    ui_panel_compact_button_render(renderer,
                                   &(UIPanelCompactButtonSpec){
                                       .rect = clearBtn,
                                       .label = "x",
                                       .active = false,
                                       .outlined = false,
                                       .use_custom_fill = true,
                                       .custom_fill = clearFill,
                                       .use_custom_outline = true,
                                       .custom_outline = palette.input_border,
                                       .tier = CORE_FONT_TEXT_SIZE_CAPTION
                                   });
    y = infoStartY + (statusLineSlots * d.info_line_gap) + d.row_gap + 2;

    const bool collapsed = control_panel_filters_collapsed();
    SDL_Rect filterHeader = {0};
    (void)ui_panel_collapsible_header_render(renderer,
                                             x,
                                             y,
                                             "Filters",
                                             collapsed,
                                             CORE_FONT_TEXT_SIZE_CAPTION,
                                             &filterHeader);
    control_panel_set_filter_header_rect(filterHeader);
    y += d.info_line_gap - 2;

    control_panel_begin_filter_button_frame();

    const int filterW = pane->w - (d.pad_left + d.pad_right);
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

    ToolPanelHeaderMetrics headerMetrics = {
        .controls_y = searchRow.y,
        .controls_h = searchRow.h,
        .info_start_y = infoStartY,
        .info_line_gap = d.info_line_gap,
        .info_line_count = statusLineSlots,
        .bottom_padding = d.row_gap + 2,
        .min_content_top = searchRow.y + searchRow.h + d.row_gap + d.info_line_gap
    };
    int minListTop = tool_panel_compute_content_top(&headerMetrics);
    int listTop = y + d.row_gap;
    if (listTop < minListTop) listTop = minListTop;
    control_panel_set_symbol_list_top(listTop);
    int listHeight = (pane->y + pane->h) - listTop;
    if (listHeight < 0) listHeight = 0;

    if (listHeight > 0) {
        tool_panel_render_split_background(renderer, pane, listTop, d.body_darken);
    }

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
