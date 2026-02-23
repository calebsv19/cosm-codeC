#include "render_tool_libraries.h"

#include "engine/Render/render_pipeline.h"
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_text_helpers.h"

#include "core/Analysis/analysis_status.h"
#include "core/Analysis/analysis_scheduler.h"
#include "ide/Panes/ToolPanels/Libraries/tool_libraries.h"
#include "ide/UI/scroll_manager.h"
#include "ide/UI/shared_theme_font_adapter.h"
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

static const char* bucket_label(LibraryBucketKind kind) {
    switch (kind) {
        case LIB_BUCKET_PROJECT:    return "Project headers";
        case LIB_BUCKET_SYSTEM:     return "System headers";
        case LIB_BUCKET_EXTERNAL:   return "External headers";
        case LIB_BUCKET_UNRESOLVED: return "Unresolved headers";
        default:                    return "Headers";
    }
}

void renderLibrariesPanel(UIPane* pane) {
    LibraryPanelState* st = &g_libraryPanelState;
    static bool scrollInit = false;
    if (!scrollInit) {
        scroll_state_init(&st->scroll, NULL);
        scrollInit = true;
    }

    RenderContext* ctx = getRenderContext();
    SDL_Renderer* renderer = ctx->renderer;

    // Title space is already drawn by the pane; reserve vertical space for it.
    // Respect pane chrome: title bar already rendered; clip starts just below it.
    const int headerHeight = LIBRARIES_HEADER_HEIGHT;
    int contentX = pane->x + 12;
    int contentY = pane->y + headerHeight;
    int contentH = pane->h - headerHeight;
    if (contentH < 0) contentH = 0;
    int viewBottom = contentY + contentH;

    SDL_Rect toggleRect = { pane->x + 12, pane->y + 24, 112, 20 };
    st->systemToggleRect = toggleRect;
    renderButton(pane,
                 toggleRect,
                 st->includeSystemHeaders ? "System: On" : "System: Off");

    SDL_Rect logsRect = { pane->x + 130, pane->y + 24, 98, 20 };
    st->logsToggleRect = logsRect;
    renderButton(pane,
                 logsRect,
                 analysis_frontend_logs_enabled() ? "Logs: On" : "Logs: Off");

    // Status indicator in header area (not clipped)
    AnalysisStatusSnapshot snap = {0};
    AnalysisSchedulerSnapshot sched = {0};
    analysis_status_snapshot(&snap);
    analysis_scheduler_snapshot(&sched);
    char statusBuf[128] = {0};
    if (snap.updating) {
        snprintf(statusBuf, sizeof(statusBuf),
                 sched.active_run_id ? "Updating (#%llu)..." : "Updating...",
                 (unsigned long long)sched.active_run_id);
    } else if (snap.last_error[0]) {
        snprintf(statusBuf, sizeof(statusBuf), "Analysis error");
    } else if (snap.has_cache) {
        snprintf(statusBuf, sizeof(statusBuf), "(cached)");
    }
    if (statusBuf[0]) {
        int tw = getTextWidth(statusBuf);
        int tx = pane->x + pane->w - tw - 16;
        int ty = pane->y + 8;
        drawText(tx, ty, statusBuf);
    }

    SDL_Color editorBg = ide_shared_theme_background_color();
    SDL_Color listBg = editorBg;
    if (same_rgb(listBg, pane->bgColor)) {
        listBg = darken_color(pane->bgColor, 14);
    }
    SDL_Rect bodyBg = {
        pane->x + 1,
        contentY,
        pane->w - 2,
        contentH
    };
    SDL_SetRenderDrawColor(renderer, listBg.r, listBg.g, listBg.b, 255);
    SDL_RenderFillRect(renderer, &bodyBg);

    SDL_Rect clip = { pane->x, contentY, pane->w, contentH };
    pushClipRect(&clip);

    st->scrollTrack = (SDL_Rect){ pane->x + pane->w - 12, contentY, 8, contentH };
    st->scrollThumb = st->scrollTrack;

    // Allow full scroll so the last line can sit at the top (add viewport height).
    float totalHeight = (float)LIBRARIES_LIST_TOP_GAP +
                        (float)(st->flatCount * LIBRARY_ROW_HEIGHT) +
                        (float)contentH;
    scroll_state_set_viewport(&st->scroll, (float)contentH);
    scroll_state_set_content_height(&st->scroll, totalHeight);
    st->scrollThumb = scroll_state_thumb_rect(&st->scroll,
                                              st->scrollTrack.x,
                                              st->scrollTrack.y,
                                              st->scrollTrack.w,
                                              st->scrollTrack.h);

    st->hoveredRow = -1;
    float offset = scroll_state_get_offset(&st->scroll);
    // Offset rows by scroll, then subtract header so row 0 starts just inside clip.
    int yStart = contentY + LIBRARIES_LIST_TOP_GAP - (int)offset;

    int mouseX = 0, mouseY = 0;
    SDL_GetMouseState(&mouseX, &mouseY);

    for (int i = 0; i < st->flatCount; ++i) {
        LibraryFlatRow* row = &st->flatRows[i];
        int rowHeight = LIBRARY_ROW_HEIGHT;
        int rowTop = yStart + i * rowHeight;
        int rowBottom = rowTop + rowHeight;
        if (rowBottom <= contentY) continue;   // entirely above clip
        if (rowTop >= viewBottom) break;       // past viewport

        // Skip rows that are only partially inside the clip; require full visibility to draw.
        if (rowTop < contentY || rowBottom > viewBottom) {
            continue;
        }

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

        int textWidth = getTextWidth(line);
        SDL_Rect box = { drawX - 6, drawY - 1, textWidth + 12, LIBRARY_ROW_HEIGHT };

        bool isSel = library_row_is_selected(i);
        if (isSel) {
            SDL_SetRenderDrawColor(renderer, 80, 80, 110, 160);
            SDL_RenderFillRect(renderer, &box);
        }

        if (mouseY >= drawY && mouseY < drawY + LIBRARY_ROW_HEIGHT &&
            mouseX >= pane->x && mouseX < pane->x + pane->w) {
            st->hoveredRow = i;
            SDL_SetRenderDrawColor(renderer, 100, 100, 140, 120);
            SDL_RenderDrawRect(renderer, &box);
        } else if (st->selectedRow == i) {
            SDL_SetRenderDrawColor(renderer, 200, 200, 240, 180);
            SDL_RenderDrawRect(renderer, &box);
        }

        drawText(drawX, drawY, line);
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
