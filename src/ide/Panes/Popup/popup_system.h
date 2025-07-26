#ifndef POPUP_SYSTEM_H
#define POPUP_SYSTEM_H

#include <SDL2/SDL.h>
#include <stdbool.h>

typedef enum {
    POPUP_TYPE_INFO,
    POPUP_TYPE_WARNING,
    POPUP_TYPE_ERROR,
    POPUP_TYPE_CONFIRMATION,
    POPUP_TYPE_FILE_DIALOG,
    POPUP_TYPE_NONE
} PopupType;

typedef struct Popup {
    PopupType type;
    const char* message;
    bool visible;
    // Future: buttons, callbacks, input fields, etc.
} Popup;

void initPopupSystem();
void enqueuePopup(PopupType type, const char* message);
void hideAllPopups();
void handlePopupEvent(SDL_Event* event);
bool isPopupVisible();

int getPopupCount();
Popup* getPopupAt(int index);


#endif

