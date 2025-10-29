#include "input_keyboard.h"
#include "core/InputManager/UserInput/rename_flow.h"
#include "core/InputManager/UserInput/rename_access.h"
#include "core/CommandBus/command_bus.h"
#include "core/CommandBus/command_metadata.h"
#include "app/GlobalInfo/core_state.h"
#include "ide/UI/layout.h"
#include "ide/UI/ui_state.h"

// This file handles SDL_KEYDOWN input → dispatchInputCommand()
// It does NOT handle behavior — that's routed by the pane’s inputHandler

static void enqueueTargetedCommand(UIPane* target, InputCommand cmd, SDL_Keymod mod) {
    InputCommandMetadata meta = {
        .cmd = cmd,
        .originRole = target ? target->role : PANE_ROLE_UNKNOWN,
        .mouseX = -1,
        .mouseY = -1,
        .keyMod = mod,
        .targetPane = target,
        .payload = NULL
    };
    enqueueCommand(meta);
}

void handleKeyboardInput(SDL_Event* event,
                         UIPane** panes, int* paneCount, bool* running) {
    if (!event || event->type != SDL_KEYDOWN) return;

    IDECoreState* core = getCoreState();
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
    if ((mod & KMOD_CTRL) && key == SDLK_1) {
        enqueueTargetedCommand(core ? core->editorPane : NULL, COMMAND_SWITCH_TAB, (SDL_Keymod)mod);
        return;
    }

    // Ctrl+R or TAB → Toggle control/tool panel visibility
    if (mod & KMOD_CTRL && key == SDLK_r) {
        enqueueTargetedCommand(uiState ? uiState->menuBar : NULL, COMMAND_TOGGLE_CONTROL_PANEL, (SDL_Keymod)mod);
        return;
    }

    if (mod & KMOD_CTRL && key == SDLK_t) {
        enqueueTargetedCommand(uiState ? uiState->menuBar : NULL, COMMAND_TOGGLE_TOOL_PANEL, (SDL_Keymod)mod);
        return;
    }

    // === FALLBACK: Send key to currently focused pane ===
    UIPane* focused = core ? core->focusedPane : NULL;
    if (focused && focused->inputHandler && focused->inputHandler->onKeyboard) {
        focused->inputHandler->onKeyboard(focused, event);
    } else {
        printf("[Keyboard] Unhandled or no focused pane: key=%d\n", key);
    }
}
