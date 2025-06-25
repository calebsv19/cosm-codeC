#ifndef RENDER_PIPELINE_H
#define RENDER_PIPELINE_H

#include "ide/UI/resize.h"
#include <SDL2/SDL.h>


// Global render context structure
typedef struct {
    SDL_Renderer* renderer;
    SDL_Window* window;
    int width, height;
} RenderContext;

// Declare the global context
extern RenderContext globalRenderContext;

// Optional accessor
RenderContext* getRenderContext();

void setRenderContext(SDL_Renderer* renderer, SDL_Window* window,
			    int width, int height);

// Forward declaration
struct UIPane;
struct IDECoreState;  // allow pointer usage


// Render pipeline lifecycle
bool initRenderPipeline();
void shutdownRenderPipeline();

// Pane rendering methods (RenderContext will be fetched internally)
void renderUIPane(struct UIPane* pane, bool hovered);
void renderMenuBar(struct UIPane* pane, bool hovered, struct IDECoreState* core);
void renderIconBar(struct UIPane* pane, bool hovered, struct IDECoreState* core);
void renderToolPanel(struct UIPane* pane, bool hovered, struct IDECoreState* core);
void renderEditor(struct UIPane* pane, bool hovered, struct IDECoreState* core);
void renderTerminal(struct UIPane* pane, bool hovered, struct IDECoreState* core);
void renderControlPanel(struct UIPane* pane, bool hovered, struct IDECoreState* core);
void renderPopupQueue(struct UIPane* pane, bool hovered, struct IDECoreState* core);

// Full render pipeline call (still needs context to handle dimensions + resizing)
void RenderPipeline_renderAll(
    struct UIPane** panes, int paneCount,
    int* lastW, int* lastH,
    ResizeZone* resizeZones, int* resizeZoneCount, struct IDECoreState* core
);



#endif

