#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/system_control.h"

#include "ide/UI/ui_state.h"
#include "ide/Panes/PaneInfo/pane.h"
#include "ide/Panes/Editor/editor_view.h"

#include "engine/Render/render_text_helpers.h"
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_font.h"
#include "ide/UI/shared_theme_font_adapter.h"
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
    SelectableTextOptions fileNameOpts = {
        .pane = pane,
        .owner = pane,
        .owner_role = pane->role,
        .x = pane->x + 8,
        .y = y,
        .maxWidth = 0,
        .text = fileName,
        .flags = TEXT_SELECTION_FLAG_SELECTABLE,
    };
    drawSelectableText(&fileNameOpts);

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
        renderButton(pane, rect, leftLabels[i]);
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
            SelectableTextOptions savedOpts = {
                .pane = pane,
                .owner = pane,
                .owner_role = pane->role,
                .x = tagX,
                .y = y,
                .maxWidth = 0,
                .text = "[saved]",
                .flags = TEXT_SELECTION_FLAG_SELECTABLE,
            };
            drawSelectableText(&savedOpts);
        } else if (file->isModified) {
            SelectableTextOptions modifiedOpts = {
                .pane = pane,
                .owner = pane,
                .owner_role = pane->role,
                .x = tagX,
                .y = y,
                .maxWidth = 0,
                .text = "[modified]",
                .flags = TEXT_SELECTION_FLAG_SELECTABLE,
            };
            drawSelectableText(&modifiedOpts);
        } else {
            file->showSavedTag = false;
        }
    }

    // 5. Right-side buttons (Build / Run / Debug / Ctrl)
    const char* rightLabels[MENU_BUTTON_RIGHT_COUNT] = { "Build", "Run", "Debug", "Ctrl" };
    for (int i = 0; i < MENU_BUTTON_RIGHT_COUNT; i++) {
        SDL_Rect rect = getRightMenuButtonRect(pane, i);
        renderButton(pane, rect, rightLabels[i]);
    }
}


void renderMenuBarCenteredFile(UIPane* pane, SDL_Renderer* renderer, struct IDECoreState* core) {
    int y = pane->y + 6;
    const char* fileName = getActiveFileName();
    const char* workspaceName = getWorkspaceDirName();

    // 1. Left side: Load, Save, [modified]  
    const char* leftLabels[MENU_BUTTON_LEFT_COUNT] = { "Load", "Save" };
    for (int i = 0; i < MENU_BUTTON_LEFT_COUNT; i++) {
        int x = pane->x + 8 + i * (LEFT_BUTTON_WIDTH + BUTTON_SPACING);
        SDL_Rect rect = { x, pane->y + BUTTON_HEIGHT_PADDING / 2,
                          LEFT_BUTTON_WIDTH, pane->h - BUTTON_HEIGHT_PADDING };
        renderButton(pane, rect, leftLabels[i]);
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
            SelectableTextOptions savedOpts = {
                .pane = pane,
                .owner = pane,
                .owner_role = pane->role,
                .x = tagX,
                .y = y,
                .maxWidth = 0,
                .text = "[saved]",
                .flags = TEXT_SELECTION_FLAG_SELECTABLE,
            };
            drawSelectableText(&savedOpts);
        } else if (file->isModified) {
            SelectableTextOptions modifiedOpts = {
                .pane = pane,
                .owner = pane,
                .owner_role = pane->role,
                .x = tagX,
                .y = y,
                .maxWidth = 0,
                .text = "[modified]",
                .flags = TEXT_SELECTION_FLAG_SELECTABLE,
            };
            drawSelectableText(&modifiedOpts);
        } else {
            file->showSavedTag = false; // auto-clear after timeout
        }
    }

    // 3. Center seam-anchored workspace + filename group
    int centerX = pane->x + pane->w / 2;
    int boxPadding = 10;
    int seamGap = 10;
    int centerLeftEdge = centerX - seamGap / 2;
    int centerRightEdge = centerX + seamGap / 2;
    int leftBoundary = pane->x + 180;
    int rightBoundary = getRightMenuButtonRect(pane, 0).x - 12;
    int leftAvail = centerLeftEdge - leftBoundary;
    int rightAvail = rightBoundary - centerRightEdge;
    if (leftAvail < 0) leftAvail = 0;
    if (rightAvail < 0) rightAvail = 0;
    int workspaceBoxW = getTextWidth(workspaceName) + boxPadding * 2;
    int nameBoxW = getTextWidth(fileName) + boxPadding * 2;
    if (workspaceBoxW > leftAvail) workspaceBoxW = leftAvail;
    if (nameBoxW > rightAvail) nameBoxW = rightAvail;

    SDL_Rect workspaceBox = {
        centerLeftEdge - workspaceBoxW,
        pane->y + BUTTON_HEIGHT_PADDING / 2,
        workspaceBoxW,
        pane->h - BUTTON_HEIGHT_PADDING
    };
    SDL_Rect nameBox = {
        centerRightEdge,
        pane->y + BUTTON_HEIGHT_PADDING / 2,
        nameBoxW,
        pane->h - BUTTON_HEIGHT_PADDING
    };

    TTF_Font* menuFont = getUIFontByTier(CORE_FONT_TEXT_SIZE_CAPTION);
    if (!menuFont) menuFont = getActiveFont();
    int textH = menuFont ? TTF_FontHeight(menuFont) : 16;
    int textYWorkspace = workspaceBox.y + (workspaceBox.h - textH) / 2;
    int textYName = nameBox.y + (nameBox.h - textH) / 2;
    int workspaceTextW = getTextWidth(workspaceName);
    int workspaceInnerW = workspaceBox.w - boxPadding * 2;
    if (workspaceInnerW < 0) workspaceInnerW = 0;
    if (workspaceTextW > workspaceInnerW) {
        size_t keep = getTextClampedLength(workspaceName, workspaceInnerW);
        workspaceTextW = getTextWidthN(workspaceName, (int)keep);
    }
    int workspaceTextX = workspaceBox.x + workspaceBox.w - boxPadding - workspaceTextW;
    int workspaceMinX = workspaceBox.x + boxPadding;
    if (workspaceTextX < workspaceMinX) workspaceTextX = workspaceMinX;

    // Draw workspace box + text
    {
        SDL_Color fill = {80, 80, 80, 100};
        SDL_Color fill_active = {100, 100, 100, 120};
        SDL_Color border = {255, 255, 255, 255};
        SDL_Color text = {255, 255, 255, 255};
        (void)text;
        ide_shared_theme_button_colors(&fill, &fill_active, &border, &text);
        SDL_SetRenderDrawColor(renderer, fill_active.r, fill_active.g, fill_active.b, 120);
        SDL_RenderFillRect(renderer, &workspaceBox);
        SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, 140);
        SDL_RenderDrawRect(renderer, &workspaceBox);
        SDL_SetRenderDrawColor(renderer, fill_active.r, fill_active.g, fill_active.b, 120);
        SDL_RenderFillRect(renderer, &nameBox);
        SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, 140);
        SDL_RenderDrawRect(renderer, &nameBox);
    }
    SelectableTextOptions workspaceOpts = {
        .pane = pane,
        .owner = pane,
        .owner_role = pane->role,
        .x = workspaceTextX,
        .y = textYWorkspace,
        .maxWidth = workspaceBox.w - boxPadding * 2,
        .text = workspaceName,
        .flags = TEXT_SELECTION_FLAG_SELECTABLE,
    };
    drawSelectableText(&workspaceOpts);

    SelectableTextOptions centeredNameOpts = {
        .pane = pane,
        .owner = pane,
        .owner_role = pane->role,
        .x = nameBox.x + boxPadding,
        .y = textYName,
        .maxWidth = nameBox.w - boxPadding * 2,
        .text = fileName,
        .flags = TEXT_SELECTION_FLAG_SELECTABLE,
    };
    drawSelectableText(&centeredNameOpts);


    // 4. Right-side buttons (Build / Run / Debug / Ctrl)
    const char* rightLabels[MENU_BUTTON_RIGHT_COUNT] = { "Build", "Run", "Debug", "Ctrl" };
    for (int i = 0; i < MENU_BUTTON_RIGHT_COUNT; i++) {
        SDL_Rect rect = getRightMenuButtonRect(pane, i);
        renderButton(pane, rect, rightLabels[i]);
    }
}
