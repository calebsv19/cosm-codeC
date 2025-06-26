#include "input_keyboard.h"
#include "core/InputManager/input_macros.h"
#include "core/InputManager/UserInput/rename_flow.h"
#include "core/CommandBus/command_bus.h"
#include "core/InputManager/UserInput/rename_access.h"

#include "ide/UI/layout.h"




// This file handles SDL_KEYDOWN input → dispatchInputCommand()
// It does NOT handle behavior — that's routed by the pane’s inputHandler



#include "core/CommandBus/command_bus.h"
#include "app/GlobalInfo/core_state.h"
#include "ide/UI/layout.h"
#include "ide/UI/ui_state.h"

#define CMD(cmd) enqueueCommandSimple(cmd)

void handleKeyboardInput(SDL_Event* event,
                         UIPane** panes, int* paneCount, bool* running) {
    if (!event || event->type != SDL_KEYDOWN) return;

    UIState* uiState = getUIState();
    SDL_Keycode key = event->key.keysym.sym;
    Uint16 mod = event->key.keysym.mod;

    // === GLOBAL COMMANDS (Always valid regardless of focused pane) ===

    // ESC exits program
    if (key == SDLK_ESCAPE) {
        *running = false;
        return;
    }


    if (isRenaming()) {
	    if (key == SDLK_RETURN) { submitRename(); return; }
	    else if (key == SDLK_ESCAPE) { cancelRename(); return; }
	    else if (key == SDLK_BACKSPACE) { handleRenameTextInput('\b'); return; }
	
	    else if (key == SDLK_LEFT) {
	        if (RENAME->cursorPosition > 0)
	            RENAME->cursorPosition--;
	        return;
	    }
	
	    else if (key == SDLK_RIGHT) {
	        if (RENAME->cursorPosition < (int)strlen(RENAME->inputBuffer))
	            RENAME->cursorPosition++;
	        return;
	    }
    }


    // Ctrl+E → Add task
/*    if ((mod & KMOD_CTRL) && key == SDLK_e) {
        CMD(COMMAND_ADD_TASK);
        return;
    }
*/
    // Ctrl+1 or Tab → Switch tab globally
    if ((mod & KMOD_CTRL && key == SDLK_1) || key == SDLK_TAB) {
        CMD(COMMAND_SWITCH_TAB);
        return;
    }

    // Ctrl+R or TAB → Toggle control/tool panel visibility
    if (mod & KMOD_CTRL && key == SDLK_r) {
        uiState->controlPanelVisible = !uiState->controlPanelVisible;
        layout_static_panes(panes, paneCount);
        return;
    }

    if (mod & KMOD_CTRL && key == SDLK_t) {
        uiState->toolPanelVisible = !uiState->toolPanelVisible;
        layout_static_panes(panes, paneCount);
        return;
    }

    // === FALLBACK: Send key to currently focused pane ===
    UIPane* focused = getCoreState()->focusedPane;
    if (focused && focused->inputHandler && focused->inputHandler->onKeyboard) {
        focused->inputHandler->onKeyboard(focused, event);
    } else {
        printf("[Keyboard] Unhandled or no focused pane: key=%d\n", key);
    }
}

