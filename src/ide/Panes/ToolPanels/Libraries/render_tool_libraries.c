#include "render_tool_libraries.h"

#include "engine/Render/render_pipeline.h"
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_text_helpers.h"
#include "engine/Render/render_font.h"

#include "core/Analysis/analysis_status.h"
#include "core/Analysis/analysis_scheduler.h"
#include "ide/Panes/ToolPanels/Libraries/tool_libraries.h"
#include "ide/Panes/ToolPanels/tool_panel_chrome.h"
#include "ide/Panes/ToolPanels/tool_panel_top_layout.h"
#include "ide/UI/row_surface.h"
#include "ide/UI/scroll_manager.h"
#include "ide/UI/shared_theme_font_adapter.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string.h>

static const char* bucket_label(LibraryBucketKind kind) {
    switch (kind) {
        case LIB_BUCKET_PROJECT:    return "Project headers";
        case LIB_BUCKET_SYSTEM:     return "System headers";
        case LIB_BUCKET_EXTERNAL:   return "External headers";
        case LIB_BUCKET_UNRESOLVED: return "Unresolved headers";
        default:                    return "Headers";
    }
}

static TTF_Font* library_row_font(void) {
    TTF_Font* font = getUIFontByTier(CORE_FONT_TEXT_SIZE_CAPTION);
    return font ? font : getActiveFont();
}

static SDL_Color library_row_color(const LibraryFlatRow* row) {
    IDEThemePalette palette = {0};
    SDL_Color primary = {230, 230, 230, 255};
    SDL_Color muted = {170, 170, 170, 255};
    if (ide_shared_theme_resolve_palette(&palette)) {
        primary = palette.text_primary;
        muted = palette.text_muted;
    }
    if (!row) return primary;
    if (row->type == LIB_NODE_USAGE) return muted;
    return primary;
}

static UIRowSurfaceRenderSpec library_row_surface_spec(bool is_selected,
                                                       bool is_primary_selected,
                                                       bool hovered) {
    UIRowSurfaceRenderSpec spec = {0};
    spec.draw_selection_fill = is_selected;
    spec.draw_hover_outline = hovered;
    spec.draw_selection_outline = is_primary_selected && !hovered;
    return spec;
}

void renderLibrariesPanel(UIPane* pane) {
    LibraryPanelState* st = libraries_panel_state();
    const int rowHeightPx = LIBRARY_ROW_HEIGHT;
    uint64_t published_index_stamp = library_index_published_stamp();
    if (st->last_published_index_stamp != published_index_stamp) {
        st->last_published_index_stamp = published_index_stamp;
        rebuildLibraryFlatRows();
    }
    static bool scrollInit = false;
    if (!scrollInit) {
        scroll_state_init(&st->scroll, NULL);
        scrollInit = true;
    }
    st->scroll.line_height_px = (float)rowHeightPx;

    RenderContext* ctx = getRenderContext();
    SDL_Renderer* renderer = ctx->renderer;

    ToolPanelLayoutDefaults d = tool_panel_layout_defaults();
    const int trackWidth = 8;
    const int trackGap = 4;
    int contentTop = tool_panel_single_row_content_top(pane);
    ToolPanelSplitLayout split = {0};
    tool_panel_compute_split_layout(pane, contentTop, &split);
    int contentX = split.body_rect.x + (d.pad_left - 1);
    int contentY = split.body_rect.y;
    int contentH = split.body_rect.h;
    int viewBottom = contentY + contentH;

    ToolPanelControlRow row = tool_panel_control_row_at(pane, pane->y + d.controls_top);
    SDL_Rect toggleRect = tool_panel_row_take_left(&row, 112);
    UIPanelTaggedRectList* controlHits = libraries_control_hits();
    ui_panel_tagged_rect_list_reset(controlHits);
    ui_panel_compact_button_render(renderer,
                                   &(UIPanelCompactButtonSpec){
                                       .rect = toggleRect,
                                       .label = st->includeSystemHeaders ? "System: On" : "System: Off",
                                       .active = st->includeSystemHeaders,
                                       .outlined = false,
                                       .use_custom_fill = false,
                                       .use_custom_outline = false,
                                       .tier = CORE_FONT_TEXT_SIZE_CAPTION
                                   });
    (void)ui_panel_tagged_rect_list_add(controlHits, LIB_TOP_CONTROL_SYSTEM_TOGGLE, toggleRect);

    SDL_Rect logsRect = tool_panel_row_take_left(&row, 98);
    ui_panel_compact_button_render(renderer,
                                   &(UIPanelCompactButtonSpec){
                                       .rect = logsRect,
                                       .label = analysis_frontend_logs_enabled() ? "Logs: On" : "Logs: Off",
                                       .active = analysis_frontend_logs_enabled(),
                                       .outlined = false,
                                       .use_custom_fill = false,
                                       .use_custom_outline = false,
                                       .tier = CORE_FONT_TEXT_SIZE_CAPTION
                                   });
    (void)ui_panel_tagged_rect_list_add(controlHits, LIB_TOP_CONTROL_LOGS_TOGGLE, logsRect);

    // Status indicator in header area (not clipped)
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
        drawText(tx, ty, statusBuf);
    }

    tool_panel_render_split_background(renderer, pane, contentTop, d.body_darken);

    st->scrollTrack = (SDL_Rect){
        split.body_rect.x + split.body_rect.w - trackWidth,
        contentY,
        trackWidth,
        contentH
    };
    st->scrollThumb = st->scrollTrack;
    SDL_Rect clip = {
        split.body_rect.x,
        contentY,
        split.body_rect.w - (trackWidth + trackGap),
        contentH
    };
    if (clip.w < 0) clip.w = 0;
    pushClipRect(&clip);

    float totalHeight = (float)(st->flatCount * rowHeightPx);
    scroll_state_set_viewport(&st->scroll, (float)contentH);
    scroll_state_set_content_height(&st->scroll,
                                    scroll_state_top_anchor_content_height(&st->scroll, totalHeight));
    st->scrollThumb = scroll_state_thumb_rect(&st->scroll,
                                              st->scrollTrack.x,
                                              st->scrollTrack.y,
                                              st->scrollTrack.w,
                                              st->scrollTrack.h);

    st->hoveredRow = -1;
    float offset = scroll_state_get_offset(&st->scroll);
    int yStart = contentY - (int)offset;
    TTF_Font* rowFont = library_row_font();
    int textHeight = rowFont ? TTF_FontHeight(rowFont) : rowHeightPx;
    if (textHeight < 1) textHeight = rowHeightPx;

    int mouseX = 0, mouseY = 0;
    SDL_GetMouseState(&mouseX, &mouseY);

    for (int i = 0; i < st->flatCount; ++i) {
        LibraryFlatRow* row = &st->flatRows[i];
        int rowHeight = rowHeightPx;
        int rowTop = yStart + i * rowHeight;
        int rowBottom = rowTop + rowHeight;
        if (rowBottom <= contentY) continue;   // entirely above clip
        if (rowTop >= viewBottom) break;       // past viewport

        int drawY = rowTop;
        int indent = row->depth * 20;
        int drawX = contentX + indent;

        const char* prefix = "    ";
        if (row->type == LIB_NODE_BUCKET) {
            bool open = st->bucketExpanded[row->bucketIndex];
            prefix = open ? "[-] " : "[+] ";
        } else if (row->type == LIB_NODE_HEADER) {
            bool open = (row->headerIndex >= 0 &&
                         row->headerIndex < (int)st->headerExpandedCount[row->bucketIndex] &&
                         st->headerExpanded[row->bucketIndex][row->headerIndex]);
            prefix = open ? "[-] " : "[+] ";
        }

        char line[1024];
        if (row->type == LIB_NODE_BUCKET) {
            int count = row->bucketHeaderCount;
            snprintf(line, sizeof(line), "%s%s (%d)", prefix, bucket_label((LibraryBucketKind)row->bucketIndex), count);
        } else if (row->type == LIB_NODE_HEADER) {
            const char* kindGlyph = (row->includeKind == LIB_INCLUDE_KIND_SYSTEM) ? "<>" : "\"\"";
            const char* unresolved = (row->bucketIndex == LIB_BUCKET_UNRESOLVED) ? " [!]" : "";
            snprintf(line, sizeof(line), "%s%s %s%s", prefix, kindGlyph,
                     row->labelPrimary ? row->labelPrimary : "(header)", unresolved);
        } else {
            snprintf(line, sizeof(line), "    %s", row->labelPrimary ? row->labelPrimary : "(usage)");
        }

        int textWidth = getTextWidthWithFont(line, rowFont);
        int textY = drawY + ((rowHeightPx - textHeight) / 2);
        SDL_Rect box = {
            drawX - 6,
            textY - 1,
            textWidth + 12,
            textHeight + 2
        };
        UIRowSurfaceLayout rowSurface = ui_row_surface_layout_from_rect(box);
        UIRowSurfaceLayout visibleSurface = {0};
        if (!ui_row_surface_clip(&rowSurface, &clip, &visibleSurface)) {
            continue;
        }

        bool isSel = library_row_is_selected(i);
        bool hovered = ui_row_surface_contains(&visibleSurface, mouseX, mouseY);
        UIRowSurfaceRenderSpec rowSpec = library_row_surface_spec(isSel,
                                                                  st->selectedRow == i,
                                                                  hovered);
        ui_row_surface_render(renderer, &visibleSurface, &rowSpec);

        if (hovered) {
            st->hoveredRow = i;
        }

        drawTextUTF8WithFontColorClipped(drawX,
                                         textY,
                                         line,
                                         rowFont,
                                         library_row_color(row),
                                         false,
                                         &clip);
    }

    // Scrollbar render
    SDL_SetRenderDrawColor(renderer, st->scroll.track_color.r, st->scroll.track_color.g,
                           st->scroll.track_color.b, st->scroll.track_color.a);
    SDL_RenderFillRect(renderer, &st->scrollTrack);
    SDL_SetRenderDrawColor(renderer, st->scroll.thumb_color.r, st->scroll.thumb_color.g,
                           st->scroll.thumb_color.b, st->scroll.thumb_color.a);
    SDL_RenderFillRect(renderer, &st->scrollThumb);
    popClipRect();
}
