

#ifndef ICON_BAR_H
#define ICON_BAR_H

#include <SDL2/SDL.h> 
#include <SDL2/SDL_rect.h>


struct UIPane;

typedef enum {
    ICON_PROJECT_FILES,
    ICON_LIBRARIES,
    ICON_BUILD_OUTPUT,
    ICON_ERRORS,
    ICON_ASSET_MANAGER,
    ICON_TASKS,
    ICON_VERSION_CONTROL,
    ICON_COUNT
} IconTool;


// Icon button dimensions (reused in render + handle)
#define ICON_BUTTON_SPACING 12
#define ICON_BUTTON_PADDING 20
#define ICON_BUTTON_TOP_OFFSET 28



SDL_Rect getIconRect(struct UIPane* pane, int index);

// Current selected icon
void setActiveIcon(IconTool tool);
IconTool getActiveIcon();

// UI usage
const char* getIconLabel(IconTool tool);
const char* getToolPanelLabel(); // returns current icon's label for tool pane title

#endif

