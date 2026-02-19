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
#include "core/Analysis/analysis_status.h"

#include <SDL2/SDL.h>

void renderControlPanelContents(UIPane* pane, bool hovered, struct IDECoreState* core) {
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer* renderer = ctx->renderer;

    renderUIPane(pane, hovered);

    int padding = 8;
    int x = pane->x + padding;
    int y = pane->y + padding;

    // Panel title
    drawText(x, y, pane->title);
    AnalysisStatusSnapshot snap = {0};
    analysis_status_snapshot(&snap);
    if (snap.updating || snap.last_error[0] || snap.has_cache) {
        char statusBuf[128] = {0};
        if (snap.updating) {
            snprintf(statusBuf, sizeof(statusBuf), "Updating...");
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
            drawText(tx, ty, statusBuf);
        }
    }
    y += 28;

    // Search bar label
    drawText(x, y, "Search:");
    y += 20;

    // Draw mock search box
    SDL_Rect searchBox = { x, y, pane->w - 2 * padding, 24 };
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 20);  // faint fill
    SDL_RenderFillRect(renderer, &searchBox);
    SDL_SetRenderDrawColor(renderer, 180, 180, 180, 255);
    SDL_RenderDrawRect(renderer, &searchBox);
    drawText(x + 6, y + 4, "type to filter...");
    y += 36;

    // Section label
    drawText(x, y, "Options:");
    y += 24;

    // Toggle 1
    drawText(x + 12, y, isLiveParseEnabled() ? "[x] Live Parse" : "[ ] Live Parse");
    y += 20;
 
    // Toggle 2
    drawText(x + 12, y, isShowInlineErrorsEnabled() ? "[x] Show Errors Inline" : "[ ] Show Errors Inline");
    y += 20;

    // Toggle 3
    drawText(x + 12, y, isShowMacrosEnabled() ? "[x] Show Macros" : "[ ] Show Macros");
    y += 20;

    int listTop = control_panel_get_symbol_list_top(pane);
    int listHeight = (pane->y + pane->h) - listTop;
    if (listHeight < 0) listHeight = 0;

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
    listPane.y = listTop;
    listPane.h = listHeight;

    if (tree) {
        renderTreePanelWithScroll(&listPane, tree, scroll, track, thumb);
    }
}
