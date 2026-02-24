

#include "menu_buttons.h"
#include "app/GlobalInfo/core_state.h"
#include "ide/UI/layout_config.h"
#include "ide/UI/ui_state.h"
#include "core/CommandBus/command_bus.h"
#include "ide/Panes/PaneInfo/pane.h"
#include "ide/Panes/Editor/editor_view.h"
#include "app/GlobalInfo/project.h"

#include <SDL2/SDL.h>
#include <string.h>


    
MenuBarLayoutMode currentMenuBarLayout = MENU_BAR_MODE_CENTER_FILENAME;
// MenuBarLayoutMode currentMenuBarLayout = MENU_BAR_MODE_STANDARD;


SDL_Rect getLeftMenuButtonRect(UIPane* pane, int index) {
    int x = LEFT_START_X(pane) + index * (LEFT_BUTTON_WIDTH + BUTTON_SPACING);
    int y = pane->y + BUTTON_HEIGHT_PADDING / 2;
    int width = LEFT_BUTTON_WIDTH;
    int height = pane->h - BUTTON_HEIGHT_PADDING;

    SDL_Rect rect = { x, y, width, height };
    return rect;
}

SDL_Rect getRightMenuButtonRect(UIPane* pane, int index) {
    int totalRightWidth = MENU_BUTTON_RIGHT_COUNT * (RIGHT_BUTTON_WIDTH + BUTTON_SPACING);
    int startX = pane->x + pane->w - totalRightWidth - 10;
    int x = startX + index * (RIGHT_BUTTON_WIDTH + BUTTON_SPACING);
    int y = pane->y + BUTTON_HEIGHT_PADDING / 2;
    int height = pane->h - BUTTON_HEIGHT_PADDING;

    SDL_Rect rect = { x, y, RIGHT_BUTTON_WIDTH, height };
    return rect;
}


const char* getActiveFileName() {
    IDECoreState* core = getCoreState();
    EditorView* activeEditorView = core->activeEditorView;

    if (!activeEditorView || activeEditorView->type != VIEW_LEAF)
        return "(no file)";

    if (activeEditorView->activeTab < 0 || activeEditorView->activeTab >= activeEditorView->fileCount)
        return "(no file)";

    OpenFile* file = activeEditorView->openFiles[activeEditorView->activeTab];
    if (!file || !file->filePath || !file->filePath[0]) return "(no file)";
    return getFileName(file->filePath);
}

const char* getWorkspaceDirName(void) {
    static char label[256];
    const char* path = getWorkspacePath();
    if (!path || !path[0]) path = projectPath;
    if (!path || !path[0]) return "(workspace)";

    const char* slash = strrchr(path, '/');
    const char* name = slash ? slash + 1 : path;
    if (!name[0] && slash && slash > path) {
        const char* end = slash - 1;
        while (end > path && *end != '/') end--;
        name = (*end == '/') ? end + 1 : path;
    }

    if (!name[0]) return "(workspace)";
    snprintf(label, sizeof(label), "%s", name);
    return label;
}




void handleCommandBuild(void) {
    printf("[Menu Bar] Build clicked\n");
    // buildAllFiles(); // placeholder
}

void handleCommandRun(void) {
    printf("[Menu Bar] Run clicked\n");
    // runExecutable(); // placeholder
}

void handleCommandDebug(void) {
    printf("[Menu Bar] Debug clicked\n");
    // toggleDebugMode(); // placeholder
}

void handleCommandToggleControlPanel(void) {
    UIState* uiState = getUIState();
    uiState->controlPanelVisible = !uiState->controlPanelVisible;
    printf("[Menu Bar] Toggled control panel to %s\n",
           uiState->controlPanelVisible ? "VISIBLE" : "HIDDEN");
}

void handleCommandLoad(void) {
    printf("[Menu Bar] Load clicked\n");
    // TODO: implement file picker or load logic
}

void handleCommandSave(void) {
    IDECoreState* core = getCoreState();
    EditorView* view = core->activeEditorView;

    if (view && view->type == VIEW_LEAF &&
        view->activeTab >= 0 && view->activeTab < view->fileCount) {

        OpenFile* file = view->openFiles[view->activeTab];

        if (file->isModified) {
            enqueueSave(file);
            printf("[Menu Bar] Save queued for: %s\n", file->filePath);
        } else {
            printf("[Menu Bar] Save skipped — file not modified: %s\n", file->filePath);
        }
    }
}




