#include "input_mouse.h"
#include "input_manager.h"
#include "input_keyboard.h"
#include "input_resize.h"
#include "input_hover.h"
#include "input_global.h"


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

    if (event->type == SDL_TEXTINPUT) {
        if (isRenaming()) {
            const char* text = event->text.text;
            for (int i = 0; text[i] != '\0'; i++) {
                handleRenameTextInput(text[i]);
            }
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

    handleResizeDragging(event, zones, zoneCount, panes, paneCountRef);

    if (event->type == SDL_MOUSEMOTION ||
        event->type == SDL_MOUSEBUTTONDOWN ||
        event->type == SDL_MOUSEBUTTONUP ||
        event->type == SDL_MOUSEWHEEL) {

        handleMouseInput(event, panes, paneCount);

        if (event->type != SDL_MOUSEMOTION) {
            SDL_Event fakeMotion;
            SDL_GetMouseState(&fakeMotion.motion.x, &fakeMotion.motion.y);
            fakeMotion.type = SDL_MOUSEMOTION;
            handleHoverUpdate(&fakeMotion, panes, paneCount);
        }
    }
}

                            
