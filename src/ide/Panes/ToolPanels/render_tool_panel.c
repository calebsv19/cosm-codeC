
#include "ide/Panes/ToolPanels/render_tool_panel.h"
#include "ide/Panes/panel_view_adapter.h"
#include "ide/Panes/ToolPanels/tool_panel_adapter.h"
#include "engine/Render/render_helpers.h"
#include "app/GlobalInfo/system_control.h"

#include "ide/Panes/PaneInfo/pane.h"
#include "ide/Panes/IconBar/icon_bar.h"




#include <SDL2/SDL.h>

void renderToolPanelContents(UIPane* pane,bool hovered, struct IDECoreState* core) {

    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
//    SDL_Renderer* renderer = ctx->renderer;

    // Draw the dynamic title for the selected icon/tool (skip for project workspace header)
    IconTool currentIcon = getActiveIcon();
    const char* label = getToolPanelLabel();
    if (currentIcon != ICON_PROJECT_FILES && label) {
        drawTextWithTier(pane->x + 8, pane->y + 6, label, CORE_FONT_TEXT_SIZE_TITLE);
    }

    // Optional clipping here for future
    // SDL_RenderSetClipRect(renderer, &contentArea);

    // Delegate rendering to the currently active tool
    renderToolPanelView(pane);

    // SDL_RenderSetClipRect(renderer, NULL); // Optional: reset clipping
}




void renderToolPanelView(UIPane* pane) {
    UIPane* previousPane = tool_panel_bind_dispatch_pane(pane);
    ui_panel_view_adapter_render(tool_panel_active_adapter(), pane, false, NULL);
    tool_panel_restore_dispatch_pane(previousPane);
}
