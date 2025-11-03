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

void beginRenameWithPrompt(const char* promptLabel,
                           const char* defaultErrorMessage,
                           const char* oldName,
                           RenameCallback callback,
                           RenameValidateFn validate,
                           void* context,
                           bool acceptUnchanged) {
    const char* initial = oldName ? oldName : "";
    strncpy(RENAME->originalName, initial, MAX_PATH_LENGTH - 1);
    RENAME->originalName[MAX_PATH_LENGTH - 1] = '\0';

    strncpy(RENAME->inputBuffer, initial, MAX_PATH_LENGTH - 1);
    RENAME->inputBuffer[MAX_PATH_LENGTH - 1] = '\0';

    const char* prompt = (promptLabel && promptLabel[0]) ? promptLabel : "Rename:";
    strncpy(RENAME->promptLabel, prompt, sizeof(RENAME->promptLabel) - 1);
    RENAME->promptLabel[sizeof(RENAME->promptLabel) - 1] = '\0';

    const char* fallbackError = (defaultErrorMessage && defaultErrorMessage[0]) ? defaultErrorMessage : "";
    strncpy(RENAME->defaultError, fallbackError, sizeof(RENAME->defaultError) - 1);
    RENAME->defaultError[sizeof(RENAME->defaultError) - 1] = '\0';

    RENAME->lastError[0] = '\0';

    RENAME->onRenameComplete = callback;
    RENAME->onValidate = validate;
    RENAME->context = context;
    RENAME->active = true;
    RENAME->cursorPosition = (int)strlen(RENAME->inputBuffer);
    RENAME->acceptUnchanged = acceptUnchanged;

    caretVisible = true;
    lastCaretBlink = SDL_GetTicks();
    renameErrorVisible = false;
    renameErrorStart = 0;

    getCoreState()->popupPaneActive = true;
}

void beginRename(const char* oldName, RenameCallback callback, RenameValidateFn validate, void* context) {
    beginRenameWithPrompt("Rename Item:", "Invalid name", oldName, callback, validate, context, false);
}

void cancelRename(void) {
    RENAME->active = false;
    getCoreState()->popupPaneActive = false;
    RENAME->lastError[0] = '\0';
    renameErrorVisible = false;
    RENAME->acceptUnchanged = false;
}

void submitRename(void) {
    if (!RENAME->active) return;

    const char* newName = RENAME->inputBuffer;
    const char* oldName = RENAME->originalName;

    if (!RENAME->acceptUnchanged && strcmp(oldName, newName) == 0) {
        RENAME->active = false;
        getCoreState()->popupPaneActive = false;
        renameErrorVisible = false;
        return;
    }

    if (RENAME->onValidate && !RENAME->onValidate(newName, RENAME->context)) {
        if (RENAME->lastError[0] == '\0' && RENAME->defaultError[0] != '\0') {
            strncpy(RENAME->lastError, RENAME->defaultError, sizeof(RENAME->lastError) - 1);
            RENAME->lastError[sizeof(RENAME->lastError) - 1] = '\0';
        }

        if (RENAME->lastError[0] != '\0') {
            renameErrorVisible = true;
            renameErrorStart = SDL_GetTicks();
        } else {
            renameErrorVisible = false;
        }
        return;
    }

    if (RENAME->onRenameComplete) {
        RENAME->onRenameComplete(oldName, newName, RENAME->context);
    }

    RENAME->active = false;
    getCoreState()->popupPaneActive = false;
    renameErrorVisible = false;
    RENAME->lastError[0] = '\0';
    RENAME->acceptUnchanged = false;
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

    if (renameErrorVisible) {
        renameErrorVisible = false;
        RENAME->lastError[0] = '\0';
    }
}

bool isRenaming(void) {
    return RENAME->active;
}

void setRenameErrorMessage(const char* message) {
    if (!message) {
        RENAME->lastError[0] = '\0';
        return;
    }

    strncpy(RENAME->lastError, message, sizeof(RENAME->lastError) - 1);
    RENAME->lastError[sizeof(RENAME->lastError) - 1] = '\0';
}

void renderRenameOverlay(void) {
    // This should eventually be replaced by a proper overlay draw function
    printf("RENAME MODE: %s -> %s\n", RENAME->originalName, RENAME->inputBuffer);
}
