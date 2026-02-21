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

    if (isPopupVisible()) {
        int count = getPopupCount();
        for (int i = 0; i < count; i++) {
            Popup* popup = getPopupAt(i);
            if (!popup || !popup->visible || !popup->message) continue;
            renderSinglePopup(popup, renderer, i);
        }
    }

    if (isRenaming()) {
        int winW = 0, winH = 0;
        SDL_GetWindowSize(ctx->window, &winW, &winH);
        renderRenamePopup(renderer, winW, winH);
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
    RenameRequest* req = RENAME;
    const char* prompt = (req && req->promptLabel[0]) ? req->promptLabel : "Rename:";
    const char* buffer = req ? req->inputBuffer : "";
    const char* errorText = (req && req->lastError[0]) ? req->lastError : "";

    int minWidth = 400;
    int margin = 80;
    int promptWidth = getTextWidth(prompt) + 24;
    int inputWidth = getTextWidth(buffer) + 24;
    int errorWidth = (renameErrorVisible && errorText[0]) ? getTextWidth(errorText) + 24 : 0;

    int maxContentWidth = promptWidth;
    if (inputWidth > maxContentWidth) maxContentWidth = inputWidth;
    if (errorWidth > maxContentWidth) maxContentWidth = errorWidth;

    if (maxContentWidth < minWidth) maxContentWidth = minWidth;

    int maxAllowed = winW - margin;
    if (maxAllowed < minWidth) maxAllowed = minWidth;
    if (maxContentWidth > maxAllowed) maxContentWidth = maxAllowed;

    int boxWidth = maxContentWidth;

    bool hasError = renameErrorVisible && errorText[0];
    int boxHeight = hasError ? 150 : 120;

    SDL_Rect box = {
        .x = (winW - boxWidth) / 2,
        .y = winH / 2 - boxHeight / 2,
        .w = boxWidth,
        .h = boxHeight
    };

    SDL_SetRenderDrawColor(renderer, 30, 30, 30, 240);
    SDL_RenderFillRect(renderer, &box);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &box);

    // Base label
    drawClippedText(box.x + 12, box.y + 18, prompt, box.w - 24);

    int textY = box.y + 48;

    if (hasError) {
        SDL_SetRenderDrawColor(renderer, 255, 70, 70, 255);
        drawClippedText(box.x + 12, box.y + 42, errorText, box.w - 24);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        textY = box.y + 66;
    }

    // Input buffer
    int textX = box.x + 12;
    drawClippedText(textX, textY, buffer, box.w - 24);

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
