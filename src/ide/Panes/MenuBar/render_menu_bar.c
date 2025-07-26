#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/system_control.h"

#include "ide/UI/ui_state.h"
#include "ide/Panes/PaneInfo/pane.h"
#include "ide/Panes/Editor/editor_view.h"

#include "engine/Render/render_text_helpers.h"
#include "engine/Render/render_helpers.h"
#include "ide/Panes/MenuBar/render_menu_bar.h"


#include "../MenuBar/menu_buttons.h" // for renderMenuBarContents



void renderMenuBarContents(UIPane* pane, struct IDECoreState* core) {
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer* renderer = ctx->renderer;

    if (currentMenuBarLayout == MENU_BAR_MODE_STANDARD) {
        renderMenuBarStandard(pane, renderer, core);
    } else {
        renderMenuBarCenteredFile(pane, renderer, core);
    }
}


void renderMenuBarStandard(UIPane* pane, SDL_Renderer* renderer, struct IDECoreState* core) {
    int y = pane->y + 6;
    const char* fileName = getActiveFileName();

    // 1. Draw file name (left-aligned)
    drawText(pane->x + 8, y, fileName);   

    // 2. Calculate button group start after file name
    int nameWidth = getTextWidth(fileName);
    int leftStartX = pane->x + 8 + nameWidth + 30;

    // 3. Draw left-side buttons (Load / Save)
    const char* leftLabels[MENU_BUTTON_LEFT_COUNT] = { "Load", "Save" };
    for (int i = 0; i < MENU_BUTTON_LEFT_COUNT; i++) {
        int x = leftStartX + i * (LEFT_BUTTON_WIDTH + BUTTON_SPACING);
        SDL_Rect rect = {
            x,
            pane->y + BUTTON_HEIGHT_PADDING / 2,
            LEFT_BUTTON_WIDTH,
            pane->h - BUTTON_HEIGHT_PADDING
        };
        renderButton(rect, leftLabels[i]);
    }

    // 4. Status tag ([modified], [saved], etc.)
    OpenFile* file = NULL;
    EditorView* view = core->activeEditorView;

    if (view &&
        view->type == VIEW_LEAF &&
        view->activeTab >= 0 &&
        view->activeTab < view->fileCount) {

        file = view->openFiles[view->activeTab];
    }

    int tagX = leftStartX + MENU_BUTTON_LEFT_COUNT * (LEFT_BUTTON_WIDTH + BUTTON_SPACING);
    if (file) {
        Uint32 now = SDL_GetTicks();

        if (file->showSavedTag && now - file->savedTagTimestamp < 1500) {
            drawText(tagX, y, "[saved]");
        } else if (file->isModified) {
            drawText(tagX, y, "[modified]");
        } else {
            file->showSavedTag = false;
        }
    }

    // 5. Right-side buttons (Build / Run / Debug / Ctrl)
    const char* rightLabels[MENU_BUTTON_RIGHT_COUNT] = { "Build", "Run", "Debug", "Ctrl" };
    for (int i = 0; i < MENU_BUTTON_RIGHT_COUNT; i++) {
        SDL_Rect rect = getRightMenuButtonRect(pane, i);
        renderButton(rect, rightLabels[i]);
    }
}


void renderMenuBarCenteredFile(UIPane* pane, SDL_Renderer* renderer, struct IDECoreState* core) {
    int y = pane->y + 6;
    const char* fileName = getActiveFileName();

    // 1. Left side: Load, Save, [modified]  
    const char* leftLabels[MENU_BUTTON_LEFT_COUNT] = { "Load", "Save" };
    for (int i = 0; i < MENU_BUTTON_LEFT_COUNT; i++) {
        int x = pane->x + 8 + i * (LEFT_BUTTON_WIDTH + BUTTON_SPACING);
        SDL_Rect rect = { x, pane->y + BUTTON_HEIGHT_PADDING / 2,
                          LEFT_BUTTON_WIDTH, pane->h - BUTTON_HEIGHT_PADDING };
        renderButton(rect, leftLabels[i]);
    }

    // 2. Modified/saved tag
    OpenFile* file = NULL;
    EditorView* view = core->activeEditorView;

    if (view &&
        view->type == VIEW_LEAF &&
        view->activeTab >= 0 &&
        view->activeTab < view->fileCount) {

        file = view->openFiles[view->activeTab];
    }

    if (file) {
        int tagX = pane->x + 8 + MENU_BUTTON_LEFT_COUNT * (LEFT_BUTTON_WIDTH + BUTTON_SPACING);
        Uint32 now = SDL_GetTicks();

        if (file->showSavedTag && now - file->savedTagTimestamp < 1500) {
            drawText(tagX, y, "[saved]");
        } else if (file->isModified) {
            drawText(tagX, y, "[modified]");   
        } else {
            file->showSavedTag = false; // auto-clear after timeout
        }
    }

    // 3. Centered filename
    int centerX = pane->x + pane->w / 2;
    int nameWidth = getTextWidth(fileName);
    int boxPadding = 10;
    SDL_Rect nameBox = {
        centerX - nameWidth / 2 - boxPadding,
        pane->y + BUTTON_HEIGHT_PADDING / 2,
        nameWidth + boxPadding * 2,
        pane->h - BUTTON_HEIGHT_PADDING
    };

    // Draw box & filename (optional highlight box here if desired)
    SDL_SetRenderDrawColor(renderer, 80, 80, 80, 100);
    SDL_RenderFillRect(renderer, &nameBox);
    drawText(centerX - nameWidth / 2, y, fileName);


    // 4. Right-side buttons (Build / Run / Debug / Ctrl)
    const char* rightLabels[MENU_BUTTON_RIGHT_COUNT] = { "Build", "Run", "Debug", "Ctrl" };
    for (int i = 0; i < MENU_BUTTON_RIGHT_COUNT; i++) {
        SDL_Rect rect = getRightMenuButtonRect(pane, i);
        renderButton(rect, rightLabels[i]);
    }
}
