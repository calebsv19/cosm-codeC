#ifndef RENAME_FLOW_H
#define RENAME_FLOW_H

#include <stdbool.h>
#include <SDL2/SDL.h>


#define MAX_PATH_LENGTH 512


extern Uint32 lastCaretBlink;
extern bool caretVisible;

extern bool renameErrorVisible;
extern Uint32 renameErrorStart;

typedef void (*RenameCallback)(const char* oldName, const char* newName, void* context);
typedef bool (*RenameValidateFn)(const char* newName, void* context);


typedef struct {
    char inputBuffer[MAX_PATH_LENGTH];
    char originalName[MAX_PATH_LENGTH];
    RenameCallback onRenameComplete;
    RenameValidateFn onValidate;          
    void* context;
    bool active;
    int cursorPosition;
    char promptLabel[128];
    char defaultError[128];
    char lastError[128];
    bool acceptUnchanged;
    bool submitWithShift;
} RenameRequest;

// Core interface


void beginRename(const char* oldName, RenameCallback callback, RenameValidateFn validate, void* context);
void beginRenameWithPrompt(const char* promptLabel,
                           const char* defaultErrorMessage,
                           const char* oldName,
                           RenameCallback callback,
                           RenameValidateFn validate,
                           void* context,
                           bool acceptUnchanged);
void cancelRename(void);
void submitRename(void);
void submitRenameWithMod(SDL_Keymod mod);
void handleRenameTextInput(char ch);
bool isRenaming(void);
void setRenameErrorMessage(const char* message);
bool tickRenameAnimation(Uint32 nowTicks);

// UI
void renderRenameOverlay(void);

#endif // RENAME_FLOW_H
