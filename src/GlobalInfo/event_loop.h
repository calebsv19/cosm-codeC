#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include <SDL2/SDL.h>
#include "PaneInfo/pane.h"
#include "UI/resize.h"


struct UIPane;

typedef struct {
    struct UIPane** panes;
    int* paneCount;
    ResizeZone* resizeZones;
    int* resizeZoneCount;
    bool* running;
    SDL_Event* event;
    int* lastW;
    int* lastH;
    float targetFrameTime;
    Uint64* lastRender;
} FrameContext;

void runFrameLoop(FrameContext* ctx, Uint64 now, float dt);

#endif

