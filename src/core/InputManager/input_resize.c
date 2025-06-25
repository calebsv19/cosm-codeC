#include "input_resize.h"
#include "ide/UI/layout.h"
#include "ide/UI/layout_config.h"

void handleResizeDragging(SDL_Event* event,
                          ResizeZone* zones, int zoneCount,
                          UIPane** panes, int* paneCount) {
    LayoutDimensions* layout = getLayoutDimensions();
                
    if (event->type == SDL_MOUSEBUTTONDOWN) {
            int mx = event->button.x;
            int my = event->button.y;
         
            for (int i = 0; i < zoneCount; i++) {
                SDL_Rect r = zones[i].bounds;
                if (mx >= r.x && mx <= r.x + r.w &&
                    my >= r.y && my <= r.y + r.h) {
    
                    gResizeDrag.active = true;
                    gResizeDrag.target = zones[i].target;
                    gResizeDrag.startMouseX = mx;
                    gResizeDrag.startMouseY = my;
 
                    LayoutDimensions* layout = getLayoutDimensions();
                
                    switch (gResizeDrag.target) {
                        case RESIZE_TOOL:
                            gResizeDrag.startValueX = layout->toolWidth;
                            break;
                        case RESIZE_CONTROL:
                            gResizeDrag.startValueX = layout->controlWidth;
                            break;
                        case RESIZE_TERMINAL:
                            gResizeDrag.startValueY = layout->terminalHeight;
                            break;
                        case RESIZE_CORNER_TOOL:
                            gResizeDrag.startValueX = layout->toolWidth;
                            gResizeDrag.startValueY = layout->terminalHeight;
                            break;
                        case RESIZE_CORNER_CONTROL: 
                            gResizeDrag.startValueX = layout->controlWidth;
                            gResizeDrag.startValueY = layout->terminalHeight;
                            break;
                        default:
                            break;
                    }
        
                    break;
                }
            }
    }
    if (event->type == SDL_MOUSEMOTION && gResizeDrag.active) {
        int dx = event->motion.x - gResizeDrag.startMouseX;
        int dy = event->motion.y - gResizeDrag.startMouseY;
        
        const int MIN_PANE_SIZE = 50;
        const int MAX_PANE_SIZE = 700;
            
        
        switch (gResizeDrag.target) {
            case RESIZE_TOOL:
                layout->toolWidth = gResizeDrag.startValueX + dx;
                if (layout->toolWidth < MIN_PANE_SIZE) layout->toolWidth = MIN_PANE_SIZE;
                if (layout->toolWidth > MAX_PANE_SIZE) layout->toolWidth = MAX_PANE_SIZE;
                break;
            case RESIZE_CONTROL:
                layout->controlWidth = gResizeDrag.startValueX - dx;
                if (layout->controlWidth < MIN_PANE_SIZE) layout->controlWidth = MIN_PANE_SIZE;
                if (layout->controlWidth > MAX_PANE_SIZE) layout->controlWidth = MAX_PANE_SIZE;
                break;
            case RESIZE_TERMINAL:
                layout->terminalHeight = gResizeDrag.startValueY - dy;
                if (layout->terminalHeight < MIN_PANE_SIZE) layout->terminalHeight = MIN_PANE_SIZE;
                if (layout->terminalHeight > MAX_PANE_SIZE) layout->terminalHeight = MAX_PANE_SIZE;
                break;
                        
            case RESIZE_CORNER_TOOL:
                layout->toolWidth = gResizeDrag.startValueX + dx;
                if (layout->toolWidth < MIN_PANE_SIZE) layout->toolWidth = MIN_PANE_SIZE;
                if (layout->toolWidth > MAX_PANE_SIZE) layout->toolWidth = MAX_PANE_SIZE;

                layout->terminalHeight = gResizeDrag.startValueY - dy;
                if (layout->terminalHeight < MIN_PANE_SIZE) layout->terminalHeight = MIN_PANE_SIZE;
                if (layout->terminalHeight > MAX_PANE_SIZE) layout->terminalHeight = MAX_PANE_SIZE;
                break;
    
            case RESIZE_CORNER_CONTROL:
                layout->controlWidth = gResizeDrag.startValueX - dx;
                if (layout->controlWidth < MIN_PANE_SIZE) layout->controlWidth = MIN_PANE_SIZE;
                if (layout->controlWidth > MAX_PANE_SIZE) layout->controlWidth = MAX_PANE_SIZE;
                          
                layout->terminalHeight = gResizeDrag.startValueY - dy;
                if (layout->terminalHeight < MIN_PANE_SIZE) layout->terminalHeight = MIN_PANE_SIZE;
                if (layout->terminalHeight > MAX_PANE_SIZE) layout->terminalHeight = MAX_PANE_SIZE;
                break;
            default: break;
        }
            
        layout_static_panes(panes, paneCount);
    }
                    
    if (event->type == SDL_MOUSEBUTTONUP) {
        gResizeDrag.active = false;
        gResizeDrag.target = RESIZE_NONE;
    }
}

