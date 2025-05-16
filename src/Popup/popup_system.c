#include "popup_system.h"
#include "../Render/render_pipeline.h"
#include "../GlobalInfo/core_state.h"

#include <stdlib.h>
#include <string.h>

#define MAX_POPUPS 8

extern bool popupPaneActive;  // defined somewhere globa

static Popup popupQueue[MAX_POPUPS];
static int popupCount = 0;

void initPopupSystem() {
    popupCount = 0;
    for (int i = 0; i < MAX_POPUPS; i++) {
        popupQueue[i].type = POPUP_TYPE_NONE;
        popupQueue[i].message = NULL;
        popupQueue[i].visible = false;
    }
}

void enqueuePopup(PopupType type, const char* message) {
    if (popupCount >= MAX_POPUPS) return;

    Popup* p = &popupQueue[popupCount++];
    p->type = type;
    p->message = message;
    p->visible = true;

    getCoreState()->popupPaneActive = true;  // make sure the pane gets included this frame
}


void hideAllPopups() {
    for (int i = 0; i < popupCount; i++) {
        popupQueue[i].visible = false;
        popupQueue[i].message = NULL;
        popupQueue[i].type = POPUP_TYPE_NONE;
    }
    popupCount = 0;

    getCoreState()->popupPaneActive = false;  // stop rendering the popup pane
}

bool isPopupVisible() {
    for (int i = 0; i < popupCount; i++) {
        if (popupQueue[i].visible) return true;
    }
    return false;
}


void handlePopupEvent(SDL_Event* event) {
    if (popupCount == 0) return;

    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_RETURN) {
        // Close only the most recent popup
        popupCount--;
        popupQueue[popupCount].visible = false;
        popupQueue[popupCount].message = NULL;
        popupQueue[popupCount].type = POPUP_TYPE_NONE;
    }
}


int getPopupCount() {
    return popupCount;
}

Popup* getPopupAt(int index) {
    if (index < 0 || index >= popupCount) return NULL;
    return &popupQueue[index];
}

