#ifndef RESIZE_H
#define RESIZE_H

#include <SDL2/SDL.h>
#include <stdbool.h>


typedef enum {
    RESIZE_TOOL,
    RESIZE_CONTROL,
    RESIZE_TERMINAL,
    RESIZE_CORNER_TOOL,
    RESIZE_CORNER_CONTROL,
    RESIZE_NONE
} ResizeTarget;

typedef struct {
    SDL_Rect bounds;
    ResizeTarget target;
} ResizeZone;

typedef struct {
    bool active;
    ResizeTarget target;
    int startMouseY;
    int startMouseX;
    int startValueX; 
    int startValueY;
} ResizeDragState;

extern ResizeDragState gResizeDrag;


void updateResizeZones(SDL_Window* window, ResizeZone* zones, int* count);

#endif

