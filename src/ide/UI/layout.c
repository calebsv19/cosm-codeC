#include "app/GlobalInfo/core_state.h"
#include "app/GlobalInfo/system_control.h"
#include "engine/Render/render_pipeline.h"
#include "ide/Panes/Editor/editor_view.h"

#include "ide/Panes/PaneInfo/pane.h"
#include "ide/Panes/Popup/popup_pane.h"

#include "layout_config.h"
#include "shared_theme_font_adapter.h"
#include "ui_state.h"
#include <SDL2/SDL.h>


bool long_terminal = true;

UITheme theme = {
	.bgMenuBar = {40, 40, 40, 255},
	.bgEditor = {20, 20, 20, 255},
	.bgIconBar = {40, 40, 40, 255},
	.bgToolBar = {30, 30, 30, 255},
	.bgControlPanel = {30, 30, 30, 255},
	.bgTerminal = {25, 25, 25, 255},
	.bgPopup = {50, 50, 50, 255},
	.border = {255, 255, 255, 255},
	.text = {255, 255, 255, 255}
};

static void apply_theme_to_pane(UIPane *pane, UIPaneRole role) {
    if (!pane) {
        return;
    }
    pane->borderColor = theme.border;
    switch (role) {
        case PANE_ROLE_MENUBAR:
            pane->bgColor = theme.bgMenuBar;
            break;
        case PANE_ROLE_EDITOR:
            pane->bgColor = theme.bgEditor;
            break;
        case PANE_ROLE_ICONBAR:
            pane->bgColor = theme.bgIconBar;
            break;
        case PANE_ROLE_TOOLPANEL:
            pane->bgColor = theme.bgToolBar;
            break;
        case PANE_ROLE_CONTROLPANEL:
            pane->bgColor = theme.bgControlPanel;
            break;
        case PANE_ROLE_TERMINAL:
            pane->bgColor = theme.bgTerminal;
            break;
        case PANE_ROLE_POPUP:
            pane->bgColor = theme.bgPopup;
            break;
        default:
            break;
    }
}

static void apply_theme_to_live_panes(UIState *ui) {
    if (!ui) {
        return;
    }
    apply_theme_to_pane(ui->menuBar, PANE_ROLE_MENUBAR);
    apply_theme_to_pane(ui->iconBar, PANE_ROLE_ICONBAR);
    apply_theme_to_pane(ui->toolPanel, PANE_ROLE_TOOLPANEL);
    apply_theme_to_pane(ui->editorPanel, PANE_ROLE_EDITOR);
    apply_theme_to_pane(ui->controlPanel, PANE_ROLE_CONTROLPANEL);
    apply_theme_to_pane(ui->terminalPanel, PANE_ROLE_TERMINAL);
    apply_theme_to_pane(ui->popup, PANE_ROLE_POPUP);
}



// === Individual Pane Constructors ===

static void updateMenuBarPane(UIPane* pane, int winW) {
    pane->x = 0;
    pane->y = 0;
    pane->w = winW;
    pane->h = PANE_MENU_HEIGHT;
    pane->visible = true;
}

static void updateIconBarPane(UIPane* pane, int winH) {
    pane->x = 0;
    pane->y = PANE_MENU_HEIGHT;
    pane->w = PANE_ICON_WIDTH;
    pane->h = winH - PANE_MENU_HEIGHT;
    pane->visible = true;
}

static void updateToolPanelPane(UIPane* pane, int winH, LayoutDimensions* layout) {
    pane->x = PANE_ICON_WIDTH;
    pane->y = PANE_MENU_HEIGHT;
    pane->w = layout->toolWidth;
    pane->h = winH - PANE_MENU_HEIGHT;
    if (long_terminal){
        pane->h -= layout->terminalHeight; 
    }
    pane->visible = true;
}

static void updateControlPanelPane(UIPane* pane, int winW, int winH, LayoutDimensions* layout) {
    pane->x = winW - layout->controlWidth;
    pane->y = PANE_MENU_HEIGHT;
    pane->w = layout->controlWidth;
    pane->h = winH - PANE_MENU_HEIGHT;
    if (long_terminal){
        pane->h -= layout->terminalHeight;
    }
    pane->visible = true;
}

static void updateEditorPane(UIPane* pane, int winW, int winH, LayoutDimensions* layout, UIState* ui) {
    int x = PANE_ICON_WIDTH + (ui->toolPanelVisible ? layout->toolWidth : 0);
    int y = PANE_MENU_HEIGHT;
    int w = winW - x - (ui->controlPanelVisible ? layout->controlWidth : 0);

    int h = winH - PANE_MENU_HEIGHT - layout->terminalHeight;
    pane->x = x;
    pane->y = y;
    pane->w = w;
    pane->h = h;
    pane->visible = true;

    pane->editorView = getCoreState()->persistentEditorView;
    if (pane->editorView) {
        pane->editorView->x = x;
        pane->editorView->y = y;
        pane->editorView->w = w;
        pane->editorView->h = h;
        pane->editorView->parentPane = pane;
    }

    getCoreState()->editorPane = pane;
}

static void updateTerminalPane(UIPane* pane, int winW, int winH, LayoutDimensions* layout, UIState* ui) {
    int x = PANE_ICON_WIDTH;
    if (!long_terminal){
    	x += (ui->toolPanelVisible ? layout->toolWidth : 0);
    }
    int y = winH - layout->terminalHeight;

    int w = winW - x;
    if (!long_terminal){
    	w -= (ui->controlPanelVisible ? layout->controlWidth : 0);
    }
    int h = layout->terminalHeight;

    pane->x = x;
    pane->y = y;
    pane->w = w;
    pane->h = h;
    pane->visible = true;
}




void layout_static_panes(UIPane* panes[], int* paneCount) {
    int winW, winH;

    RenderContext* ctx = getRenderContext();
    SDL_Window* window = ctx->window;    

    SDL_GetWindowSize(window, &winW, &winH);

    LayoutDimensions* layout = getLayoutDimensions();
    UIState* ui = getUIState();

    ide_shared_theme_apply(&theme);
    apply_theme_to_live_panes(ui);
    *paneCount = 0;

    syncPopupPane(panes, paneCount);

    if (ui->menuBar && *paneCount < MAX_PANES) {
        updateMenuBarPane(ui->menuBar, winW);
        panes[(*paneCount)++] = ui->menuBar;
    }

    if (ui->iconBar && *paneCount < MAX_PANES) {
        updateIconBarPane(ui->iconBar, winH);
        panes[(*paneCount)++] = ui->iconBar;
    }

    if (ui->toolPanelVisible && ui->toolPanel && *paneCount < MAX_PANES) {
        updateToolPanelPane(ui->toolPanel, winH, layout);
        panes[(*paneCount)++] = ui->toolPanel;
    }

    if (ui->controlPanelVisible && ui->controlPanel && *paneCount < MAX_PANES) {
        updateControlPanelPane(ui->controlPanel, winW, winH, layout);
        panes[(*paneCount)++] = ui->controlPanel;
    }

    if (ui->editorPanel && *paneCount < MAX_PANES) {
        updateEditorPane(ui->editorPanel, winW, winH, layout, ui);
        panes[(*paneCount)++] = ui->editorPanel;
    }

    if (ui->terminalPanel && *paneCount < MAX_PANES) {
        updateTerminalPane(ui->terminalPanel, winW, winH, layout, ui);
        panes[(*paneCount)++] = ui->terminalPanel;
    }

    if (ui->popup && *paneCount < MAX_PANES) {
        panes[(*paneCount)++] = ui->popup;
    }
}
