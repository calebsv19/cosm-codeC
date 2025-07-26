#include "icon_bar.h"
#include "ide/Panes/PaneInfo/pane.h"
#include <SDL2/SDL.h>
#include <string.h>
#include <stdio.h> // for printf

static IconTool activeTool = ICON_PROJECT_FILES;

SDL_Rect getIconRect(UIPane* pane, int index) {
    int iconSize = pane->w - ICON_BUTTON_PADDING;
    int spacing = ICON_BUTTON_SPACING;
    int startY = pane->y + ICON_BUTTON_TOP_OFFSET;
    int centerX = pane->x + pane->w / 2;
    int iconX = centerX - iconSize / 2;
    int iconY = startY + index * (iconSize + spacing);

    SDL_Rect rect = { iconX, iconY, iconSize, iconSize };
    return rect;
}





void setActiveIcon(IconTool tool) {
    if (tool >= 0 && tool < ICON_COUNT) {
        activeTool = tool;
    }
}

IconTool getActiveIcon() {
    return activeTool;
}

const char* getIconLabel(IconTool tool) {
    switch (tool) {
        case ICON_PROJECT_FILES: return "Project";
        case ICON_LIBRARIES:     return "Libraries";
        case ICON_BUILD_OUTPUT:  return "Build Output";
        case ICON_ERRORS:        return "Errors";
        case ICON_ASSET_MANAGER: return "Assets";
        case ICON_TASKS:         return "Tasks / TODO";
        case ICON_VERSION_CONTROL: return "Version Control";
        default: return "";
    }
}

const char* getToolPanelLabel() {
    return getIconLabel(getActiveIcon());
}









