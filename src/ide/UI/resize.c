#include "resize.h"
#include "layout_config.h"
#include "ui_state.h"
#include <stdbool.h>

ResizeDragState gResizeDrag = {
    .active = false,
    .target = RESIZE_NONE,
    .startMouseY = 0,
    .startMouseX = 0,
    .startValueX = 0,
    .startValueY = 0
};


void updateResizeZones(SDL_Window* window, ResizeZone* zones, int* count) {
    int winW, winH;
    SDL_GetWindowSize(window, &winW, &winH);

    UIState* ui = getUIState();
    LayoutDimensions* layout = getLayoutDimensions();

    int hitboxWidth = 10;

    int idx = 0;

    if (ui->toolPanelVisible) {
	zones[idx].target = RESIZE_CORNER_TOOL;
        zones[idx].bounds = (SDL_Rect){
            .x = PANE_ICON_WIDTH + layout->toolWidth - hitboxWidth,
            .y = winH - layout->terminalHeight - hitboxWidth,
            .w = hitboxWidth * 2,
            .h = hitboxWidth * 2
        };
        idx++;

        zones[idx].target = RESIZE_TOOL;
        zones[idx].bounds = (SDL_Rect){
            .x = PANE_ICON_WIDTH + layout->toolWidth - hitboxWidth / 2,
            .y = PANE_MENU_HEIGHT,
            .w = hitboxWidth,
            .h = winH - layout->terminalHeight - PANE_MENU_HEIGHT
        };
        idx++;

    }

    if (ui->controlPanelVisible) {
	zones[idx].target = RESIZE_CORNER_CONTROL;
    	zones[idx].bounds = (SDL_Rect){
            .x = winW - layout->controlWidth - hitboxWidth,
            .y = winH - layout->terminalHeight - hitboxWidth,
            .w = hitboxWidth * 2,
            .h = hitboxWidth * 2
        };
        idx++;


        zones[idx].target = RESIZE_CONTROL;
        zones[idx].bounds = (SDL_Rect){
            .x = winW - layout->controlWidth - hitboxWidth / 2,
            .y = PANE_MENU_HEIGHT,
            .w = hitboxWidth,
            .h = winH - layout->terminalHeight - PANE_MENU_HEIGHT
        };
        idx++;
    	
    }

    // Always show terminal resize zone
    zones[idx].target = RESIZE_TERMINAL;
    zones[idx].bounds = (SDL_Rect){
        .x = PANE_ICON_WIDTH + (ui->toolPanelVisible ? layout->toolWidth : 0),
        .y = winH - layout->terminalHeight - 2,
        .w = winW - (PANE_ICON_WIDTH + (ui->toolPanelVisible ? layout->toolWidth : 0) + 
			(ui->controlPanelVisible ? layout->controlWidth : 0)),
        .h = 4
    };
    idx++;

    *count = idx;
}

