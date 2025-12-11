#include "render_tool_libraries.h"

#include "engine/Render/render_pipeline.h"
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_text_helpers.h"

#include "core/Analysis/library_index.h"
#include "ide/Panes/ToolPanels/Libraries/tool_libraries.h"
#include "ide/UI/scroll_manager.h"
#include <SDL2/SDL.h>
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
    const int headerHeight = 32;
    int contentX = pane->x + 12;
    int contentY = pane->y + headerHeight;
    int contentH = pane->h - headerHeight;
    if (contentH < 0) contentH = 0;
    int viewBottom = contentY + contentH;

    SDL_Rect clip = { pane->x, contentY, pane->w, contentH };
    pushClipRect(&clip);

    st->scrollTrack = (SDL_Rect){ pane->x + pane->w - 12, contentY, 8, contentH };
    st->scrollThumb = st->scrollTrack;

    // Allow full scroll so the last line can sit at the top (add viewport height).
    float totalHeight = (float)(st->flatCount * LIBRARY_ROW_HEIGHT) + (float)contentH;
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
    int yStart = contentY - (int)offset;

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
            const LibraryBucket* b = library_index_get_bucket((size_t)row->bucketIndex);
            int count = b ? (int)b->header_count : 0;
            snprintf(line, sizeof(line), "%s%s (%d)", prefix, bucket_label((LibraryBucketKind)row->bucketIndex), count);
        } else if (row->type == LIB_NODE_HEADER) {
            const LibraryHeader* h = NULL;
            if (row->bucketIndex >= 0) {
                const LibraryBucket* b = library_index_get_bucket((size_t)row->bucketIndex);
                h = library_index_get_header(b, (size_t)row->headerIndex);
            }
            const char* kindGlyph = h ? ((h->kind == LIB_INCLUDE_KIND_SYSTEM) ? "<>" : "\"\"") : "";
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
