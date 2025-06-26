#include "rename_flow.h"
#include "ide/Panes/ToolPanels/Project/rename_callbacks.h"
#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/project.h"

#include "core/InputManager/UserInput/rename_access.h"

#include <string.h>
#include <stdio.h>
#include <SDL2/SDL.h>



Uint32 lastCaretBlink = 0;
bool caretVisible = true;

bool renameErrorVisible = false;
Uint32 renameErrorStart = 0;

void beginRename(const char* oldName, RenameCallback callback, void* context) {
    strncpy(RENAME->originalName, oldName, MAX_PATH_LENGTH);
    strncpy(RENAME->inputBuffer, oldName, MAX_PATH_LENGTH);
    RENAME->onRenameComplete = callback;
    RENAME->context = context;
    RENAME->active = true;
    RENAME->cursorPosition = (int)strlen(oldName);

    getCoreState()->popupPaneActive = true;
}

void cancelRename(void) {
    RENAME->active = false;
    getCoreState()->popupPaneActive = false;
}

void submitRename(void) {
    if (!RENAME->active) return;

    const char* newName = RENAME->inputBuffer;
    const char* oldName = RENAME->originalName;

    if (strcmp(oldName, newName) == 0) {
        RENAME->active = false;
        getCoreState()->popupPaneActive = false;
        renameErrorVisible = false;
        return;
    }

    if (!isRenameValid(newName, (DirEntry*)RENAME->context)) {
        renameErrorVisible = true;
        renameErrorStart = SDL_GetTicks();
        return;
    }

    if (RENAME->onRenameComplete) {
        RENAME->onRenameComplete(oldName, newName, RENAME->context);
    }

    RENAME->active = false;
    getCoreState()->popupPaneActive = false;
    renameErrorVisible = false;
}


void handleRenameTextInput(char ch) {
    if (!RENAME->active) return;

    size_t len = strlen(RENAME->inputBuffer);
    int pos = RENAME->cursorPosition;

    if (ch == '\b') {
        if (pos > 0) {
            // Shift everything left
            memmove(&RENAME->inputBuffer[pos - 1],
                    &RENAME->inputBuffer[pos],
                    len - pos + 1);
            RENAME->cursorPosition--;
        }
    } else if (ch >= 32 && ch < 127 && len < MAX_PATH_LENGTH - 1) {
        // Shift everything right
        memmove(&RENAME->inputBuffer[pos + 1],
                &RENAME->inputBuffer[pos],
                len - pos + 1);
        RENAME->inputBuffer[pos] = ch;
        RENAME->cursorPosition++;
    }
}

bool isRenaming(void) {
    return RENAME->active;
}

void renderRenameOverlay(void) {
    // This should eventually be replaced by a proper overlay draw function
    printf("RENAME MODE: %s -> %s\n", RENAME->originalName, RENAME->inputBuffer);
}

