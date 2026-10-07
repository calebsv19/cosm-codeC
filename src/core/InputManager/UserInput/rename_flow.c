#include "rename_flow.h"
#include "ide/UI/panel_text_edit.h"
#include "ide/UI/button_scope.h"
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

bool tickRenameAnimation(Uint32 nowTicks) {
    bool changed = false;

    if (!RENAME->active) {
        return false;
    }

    Uint32 elapsed = nowTicks - lastCaretBlink;
    if (caretVisible && elapsed > 550) {
        caretVisible = false;
        lastCaretBlink = nowTicks;
        changed = true;
    } else if (!caretVisible && elapsed > 250) {
        caretVisible = true;
        lastCaretBlink = nowTicks;
        changed = true;
    }

    if (renameErrorVisible && (nowTicks - renameErrorStart > 2000)) {
        renameErrorVisible = false;
        changed = true;
    }

    return changed;
}

void beginRenameWithPrompt(const char* promptLabel,
                           const char* defaultErrorMessage,
                           const char* oldName,
                           RenameCallback callback,
                           RenameValidateFn validate,
                           void* context,
                           bool acceptUnchanged) {
    const char* initial = oldName ? oldName : "";
    UIPanelTextEditBuffer buffer={RENAME->inputBuffer,sizeof(RENAME->inputBuffer),&RENAME->cursorPosition,&RENAME->retainedEdit};
    if(!ui_panel_text_edit_set_text(&buffer,initial)){
        cancelRename();
        return;
    }
    memmove(RENAME->originalName,RENAME->inputBuffer,strlen(RENAME->inputBuffer)+1);
    ide_button_scopes_cancel_except(NULL);

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
    RENAME->submitWithShift = false;

    caretVisible = true;
    lastCaretBlink = SDL_GetTicks();
    renameErrorVisible = false;
    renameErrorStart = 0;

    getCoreState()->popupPaneActive = true;
    requestFullRedraw(RENDER_INVALIDATION_OVERLAY);
}

void beginRename(const char* oldName, RenameCallback callback, RenameValidateFn validate, void* context) {
    beginRenameWithPrompt("Rename Item:", "Invalid name", oldName, callback, validate, context, false);
}

void cancelRenameComposition(void) {
    kit_ui_text_cancel_composition(&RENAME->retainedEdit);
    requestFullRedraw(RENDER_INVALIDATION_OVERLAY);
}

void cancelRename(void) {
    kit_ui_text_cancel_composition(&RENAME->retainedEdit);
    RENAME->active = false;
    getCoreState()->popupPaneActive = false;
    RENAME->lastError[0] = '\0';
    renameErrorVisible = false;
    RENAME->acceptUnchanged = false;
    RENAME->submitWithShift = false;
    requestFullRedraw(RENDER_INVALIDATION_OVERLAY);
}

void submitRename(void) {
    submitRenameWithMod((SDL_Keymod)0);
}

void submitRenameWithMod(SDL_Keymod mod) {
    if (!RENAME->active) return;
    RENAME->submitWithShift = (mod & KMOD_SHIFT) != 0;

    if(RENAME->retainedEdit.composition[0])return; /* Preedit is not a submitted name. */
    const char* newName = RENAME->inputBuffer;
    const char* oldName = RENAME->originalName;

    if (!RENAME->acceptUnchanged && strcmp(oldName, newName) == 0) {
        kit_ui_text_cancel_composition(&RENAME->retainedEdit);
    RENAME->active = false;
        getCoreState()->popupPaneActive = false;
        renameErrorVisible = false;
        requestFullRedraw(RENDER_INVALIDATION_OVERLAY);
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
            requestFullRedraw(RENDER_INVALIDATION_OVERLAY);
        } else {
            renameErrorVisible = false;
        }
        return;
    }

    if (RENAME->onRenameComplete) {
        RENAME->onRenameComplete(oldName, newName, RENAME->context);
    }

    kit_ui_text_cancel_composition(&RENAME->retainedEdit);
    RENAME->active = false;
    getCoreState()->popupPaneActive = false;
    renameErrorVisible = false;
    RENAME->lastError[0] = '\0';
    RENAME->acceptUnchanged = false;
    RENAME->submitWithShift = false;
    requestFullRedraw(RENDER_INVALIDATION_OVERLAY);
}


static void rename_edit_changed(void) {
    if(renameErrorVisible){renameErrorVisible=false;RENAME->lastError[0]='\0';}
    lastCaretBlink=SDL_GetTicks();caretVisible=true;
    requestFullRedraw(RENDER_INVALIDATION_OVERLAY);
}
bool handleRenameUtf8TextInput(const char* text) {
    if(!RENAME->active)return false;
    UIPanelTextEditBuffer buffer={RENAME->inputBuffer,sizeof(RENAME->inputBuffer),&RENAME->cursorPosition,&RENAME->retainedEdit};
    if(!ui_panel_text_edit_insert(&buffer,text))return false;
    rename_edit_changed();return true;
}
bool handleRenameEditingKeyWithMod(SDL_Keycode key,SDL_Keymod mod) {
    if(!RENAME->active)return false;
    UIPanelTextEditBuffer buffer={RENAME->inputBuffer,sizeof(RENAME->inputBuffer),&RENAME->cursorPosition,&RENAME->retainedEdit};
    if(!ui_panel_text_edit_handle_keydown_mod(&buffer,key,mod))return false;
    rename_edit_changed();return true;
}
bool handleRenameEditingKey(SDL_Keycode key) { return handleRenameEditingKeyWithMod(key,KMOD_NONE); }
bool handleRenameTextEvent(const SDL_Event* event) {
    if(!RENAME->active)return false;
    UIPanelTextEditBuffer buffer={RENAME->inputBuffer,sizeof(RENAME->inputBuffer),&RENAME->cursorPosition,&RENAME->retainedEdit};
    if(!ui_panel_text_edit_handle_text_input(&buffer,event))return false;
    rename_edit_changed();return true;
}

void handleRenameTextInput(char ch) {
    if(ch=='\b'){(void)handleRenameEditingKey(SDLK_BACKSPACE);return;}
    if(ch>=32 && ch<127){char text[2]={ch,0};(void)handleRenameUtf8TextInput(text);}
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
