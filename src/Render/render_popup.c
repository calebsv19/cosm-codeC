#include "render_popup.h"
#include "render_pipeline.h"
#include "render_text_helpers.h"
#include "render_helpers.h"
#include "../GlobalInfo/system_control.h"


#include "../Popup/popup_system.h"   // for renderPopup(), isPopupVisible()
#include <SDL2/SDL.h>



void renderPopupQueueContents() {
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer* renderer = ctx->renderer;	

    if (!isPopupVisible()) return;

    int count = getPopupCount();
    for (int i = 0; i < count; i++) {
        Popup* popup = getPopupAt(i);
        if (!popup || !popup->visible || !popup->message) continue;

        renderSinglePopup(popup, renderer, i);
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



