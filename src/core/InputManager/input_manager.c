#include "input_mouse.h"
#include "input_manager.h"
#include "input_keyboard.h"
#include "input_resize.h"
#include "input_hover.h"
#include "input_global.h"
#include "ide/Panes/MenuBar/input_menu_bar.h"
#include "ide/Panes/IconBar/input_icon_bar.h"
#include "ide/Panes/IconBar/icon_bar.h"
#include "ide/Panes/ToolPanels/Project/project_toolbar.h"
#include "ide/UI/button_scope.h"


#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/project.h"

#include "core/CommandBus/command_bus.h"
#include "core/InputManager/UserInput/rename_flow.h"

#include "ide/Panes/Editor/editor.h"
#include "ide/Panes/Editor/editor_view.h"

#include "ide/Panes/PaneInfo/pane.h"
#include "ide/UI/layout_config.h"
#include "ide/UI/layout.h"

#include <stdio.h>
#include <string.h>




void handleInput(SDL_Event* event,
                 UIPane** panes, int paneCount,
                 ResizeZone* zones, int zoneCount,
                 int* paneCountRef, bool* running) {

    handleWindowGlobalEvents(event, panes, paneCountRef, running);  // simplified

    if (event->type == SDL_TEXTINPUT || event->type == SDL_TEXTEDITING) {
        if (isRenaming()) {
            (void)handleRenameTextEvent(event);
            return;  // steal input from other systems
        } else {
            UIPane* focused = getCoreState()->focusedPane;
            if (focused && focused->inputHandler && focused->inputHandler->onTextInput) {
                focused->inputHandler->onTextInput(focused, event);
                return;
            }
        }
    }

    if (event->type == SDL_KEYDOWN) {
        handleKeyboardInput(event, panes, paneCountRef, running);
    }

    if (event->type == SDL_KEYUP) {
        UIPane* focused = getCoreState()->focusedPane;
        if (focused && focused->role == PANE_ROLE_MENUBAR) handleMenuBarKeyboardInput(focused, event);
        if (focused && focused->role == PANE_ROLE_ICONBAR) handleIconBarKeyboardInput(focused, event);
        if (focused && (focused->role == PANE_ROLE_CONTROLPANEL ||
            (focused->role == PANE_ROLE_TOOLPANEL && (getActiveIcon()==ICON_PROJECT_FILES || getActiveIcon()==ICON_ERRORS || getActiveIcon()==ICON_VERSION_CONTROL || getActiveIcon()==ICON_LIBRARIES || getActiveIcon()==ICON_TASKS || getActiveIcon()==ICON_ASSET_MANAGER))))
            if (focused->inputHandler && focused->inputHandler->onKeyboard)
                focused->inputHandler->onKeyboard(focused, event);
    }
    bool was_resizing = gResizeDrag.active;
    handleResizeDragging(event, zones, zoneCount, panes, paneCountRef);
    if (gResizeDrag.active || was_resizing) {
        ide_button_scopes_cancel_except(NULL);
        cancelMouseInputCapture();
        ide_menu_bar_cancel();
        return;
    }

    if (event->type == SDL_MOUSEMOTION ||
        event->type == SDL_MOUSEBUTTONDOWN ||
        event->type == SDL_MOUSEBUTTONUP ||
        event->type == SDL_MOUSEWHEEL) {

        handleMouseInput(event, panes, paneCount);
        handleHoverUpdate(event, panes, paneCount);
    }
}

                            
