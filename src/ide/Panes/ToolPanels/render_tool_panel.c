
#include "ide/Panes/ToolPanels/render_tool_panel.h"
#include "engine/Render/render_helpers.h"
#include "app/GlobalInfo/system_control.h"

#include "ide/Panes/PaneInfo/pane.h"
#include "ide/Panes/IconBar/icon_bar.h"

#include "ide/Panes/ToolPanels/Project/render_tool_project.h"
#include "ide/Panes/ToolPanels/Libraries/render_tool_libraries.h"
#include "ide/Panes/ToolPanels/BuildOutput/render_tool_build_output.h"
#include "ide/Panes/ToolPanels/Errors/render_tool_errors.h"
#include "ide/Panes/ToolPanels/Assets/render_tool_assets.h"
#include "ide/Panes/ToolPanels/Tasks/render_tool_tasks.h"
#include "ide/Panes/ToolPanels/Git/render_tool_git.h"




#include <SDL2/SDL.h>

void renderToolPanelContents(UIPane* pane,bool hovered, struct IDECoreState* core) {

    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
//    SDL_Renderer* renderer = ctx->renderer;

    renderUIPane(pane, hovered);

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
    IconTool current = getActiveIcon();

    switch (current) {
        case ICON_PROJECT_FILES:
            renderProjectFilesPanel(pane);
            break;
        case ICON_LIBRARIES:
            renderLibrariesPanel(pane);
            break;
        case ICON_BUILD_OUTPUT:
            renderBuildOutputPanel(pane);
            break;
        case ICON_ERRORS:
            renderErrorsPanel(pane);
            break;
        case ICON_ASSET_MANAGER:
            renderAssetManagerPanel(pane);
            break;
        case ICON_TASKS:
            renderTasksPanel(pane);
            break;
        case ICON_VERSION_CONTROL:
            renderGitPanel(pane);
            break;
        default:
            break;
    }
}
