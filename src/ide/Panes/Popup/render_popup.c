#include "ide/Panes/Popup/render_popup.h"
#include "engine/Render/render_pipeline.h"
#include "engine/Render/render_text_helpers.h"
#include "engine/Render/render_helpers.h"
#include "app/GlobalInfo/system_control.h"
#include "app/GlobalInfo/core_state.h"
#include "core/InputManager/UserInput/rename_flow.h"
#include "core/InputManager/UserInput/rename_access.h"

#include "../Popup/popup_system.h"   // for renderPopup(), isPopupVisible()

void renderPopupQueueContents() {
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;

    SDL_Renderer* renderer = ctx->renderer;
    bool drewAnything = false;

    if (isPopupVisible()) {
        int count = getPopupCount();
        for (int i = 0; i < count; i++) {
            Popup* popup = getPopupAt(i);
            if (!popup || !popup->visible || !popup->message) continue;
            renderSinglePopup(popup, renderer, i);
            drewAnything = true;
        }
    }

    if (isRenaming()) {
        int winW = 0, winH = 0;
        SDL_GetWindowSize(ctx->window, &winW, &winH);
        renderRenamePopup(renderer, winW, winH);
        drewAnything = true;
    }
}

void renderSinglePopup(Popup* popup, SDL_Renderer* renderer, int index) {
    int baseY = 300 + index * 220;

    SDL_Rect popupBox = { 400, baseY, 600, 200 };
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 230);
    SDL_RenderFillRect(renderer, &popupBox);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &popupBox);

    drawClippedText(popupBox.x + 20, popupBox.y + 40, popup->message, popupBox.w - 40);
}


void renderRenamePopup(SDL_Renderer* renderer, int winW, int winH) {
    SDL_Rect box = {
        .x = winW / 2 - 200,
        .y = winH / 2 - 60,
        .w = 400,
        .h = 120
    };

    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 240);
    SDL_RenderFillRect(renderer, &box);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &box);

    // Base label
    drawClippedText(box.x + 12, box.y + 18, "Rename File:", box.w - 24);

    // Error text (if applicable)
    if (renameErrorVisible) {
        Uint32 now = SDL_GetTicks();
        if (now - renameErrorStart > 2000) {
            renameErrorVisible = false;
        } else {
            SDL_SetRenderDrawColor(renderer, 255, 70, 70, 255);  // red
            drawClippedText(box.x + 150, box.y + 18, "(Name already exists)", box.w - 160);

            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        }
    }

    // Input buffer
    const char* buffer = getCoreState()->renameFlow.inputBuffer;
    int textX = box.x + 12;
    int textY = box.y + 48;
    drawClippedText(textX, textY, buffer, box.w - 24);

    // Caret blinking
    Uint32 now = SDL_GetTicks();
    Uint32 elapsed = now - lastCaretBlink;
    if (caretVisible && elapsed > 550) {
        caretVisible = false; 
        lastCaretBlink = now;
    } else if (!caretVisible && elapsed > 250) {
        caretVisible = true;
        lastCaretBlink = now;
    }

    if (caretVisible) {
	char temp[MAX_PATH_LENGTH];
	strncpy(temp, buffer, RENAME->cursorPosition);
	temp[RENAME->cursorPosition] = '\0';
	int textW = getTextWidth(temp); // width of text up to caret

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawLine(renderer,
            textX + textW + 1, textY,
            textX + textW + 1, textY + 18);
    }
}
