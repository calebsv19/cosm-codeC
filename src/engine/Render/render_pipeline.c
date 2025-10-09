#include "build_config.h"
#include "engine/Render/render_pipeline.h"
#include "engine/Render/render_helpers.h"
#include "engine/Render/render_text_helpers.h"
#include "engine/Render/render_font.h"

#include "ide/Panes/MenuBar/render_menu_bar.h"
#include "ide/Panes/Editor/Render/render_editor.h"
#include "ide/Panes/Terminal/render_terminal.h"

#include "ide/Panes/IconBar/render_icon_bar.h"
#include "ide/Panes/ToolPanels/render_tool_panel.h"
#include "ide/Panes/ControlPanel/render_control_panel.h"
#include "ide/Panes/Popup/render_popup.h"


// External dependencies
#include "app/GlobalInfo/core_state.h"

#include "ide/Panes/PaneInfo/pane.h"
#include "ide/UI/layout.h"
#include "ide/Panes/Editor/editor_view.h"
#include "ide/Panes/MenuBar/menu_buttons.h"



// TimerHud extension
#include "engine/TimerHUD/src/api/time_scope.h"


#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string.h>
#include <stdio.h>




RenderContext globalRenderContext;

RenderContext* getRenderContext() {
    return &globalRenderContext;
}

void setRenderContext(SDL_Renderer* renderer, SDL_Window* window,
                      int width, int height) {
    globalRenderContext.renderer = renderer;
    globalRenderContext.window = window;
    globalRenderContext.width = width;
    globalRenderContext.height = height;
}




// ========================================
//   SECTION: Initialization


bool initRenderPipeline() {
    if (getCoreState()->timerHudEnabled) {
        ts_init();
    }
    return initFontSystem();
}

void shutdownRenderPipeline() {
    shutdownFontSystem();
}





//   	SECTION: Initialization
// ========================================
//      SECTION: Rendering Branches





void renderMenuBar(UIPane* pane, bool hovered, IDECoreState* core) {
    renderUIPane(pane, hovered);
    renderMenuBarContents(pane, core);
}

void renderIconBar(UIPane* pane, bool hovered, IDECoreState* core) {
    renderUIPane(pane, hovered);
    renderIconBarContents(pane, hovered, core);
}

void renderToolPanel(UIPane* pane, bool hovered, IDECoreState* core) {
    renderUIPane(pane, hovered);
    renderToolPanelContents(pane, hovered,core);
}

void renderEditor(UIPane* pane, bool hovered, IDECoreState* core) {
    renderUIPane(pane, hovered);
    renderEditorViewContents(pane, hovered, core);
}

void renderTerminal(UIPane* pane, bool hovered, IDECoreState* core) {
    renderUIPane(pane, hovered);
    renderTerminalContents(pane, hovered, core);
}

void renderControlPanel(UIPane* pane, bool hovered, IDECoreState* core) {
    renderUIPane(pane,hovered);
    renderControlPanelContents(pane, hovered, core);
}

void renderPopupQueue(UIPane* pane, bool hovered, IDECoreState* core) {
    renderUIPane(pane, hovered);
    renderPopupQueueContents();
}





//      SECTION: Rendering Branches
// ========================================
//      SECTION: Main Rendering


 
 
 
 

// Default render function
void renderUIPane(UIPane* pane, bool hovered) {
    if (!pane || !pane->visible) return;

    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer* renderer = ctx->renderer;

    SDL_SetRenderDrawColor(renderer,
        pane->bgColor.r, pane->bgColor.g, pane->bgColor.b, pane->bgColor.a);

    SDL_Rect rect = { pane->x, pane->y, pane->w, pane->h };
    SDL_RenderFillRect(renderer, &rect);

    SDL_Color borderColor = hovered
        ? (SDL_Color){140, 150, 240, 255}
        : pane->borderColor;

    SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);
    SDL_RenderDrawRect(renderer, &rect);
}




void RenderPipeline_renderAll(UIPane** panes, int paneCount,
                              int* lastW, int* lastH, ResizeZone* resizeZones,
                              int* resizeZoneCount, IDECoreState* core){

    const bool timerHudActive = (core && core->timerHudEnabled);

    if (timerHudActive) {
        ts_start_timer("Render");
    }

    int winW, winH;
    RenderContext* ctx = getRenderContext();
    if (!ctx) {
        if (timerHudActive) {
            ts_stop_timer("Render");
        }
        return;
    }

#if USE_VULKAN
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkExtent2D extent = {0, 0};
#endif

    SDL_GetWindowSize(ctx->window, &winW, &winH);

    int drawableW = winW;
    int drawableH = winH;
#if USE_VULKAN
    SDL_Vulkan_GetDrawableSize(ctx->window, &drawableW, &drawableH);
#else
    SDL_GL_GetDrawableSize(ctx->window, &drawableW, &drawableH);

    float scaleX = (float)drawableW / (float)winW;
    float scaleY = (float)drawableH / (float)winH;
    SDL_RenderSetScale(ctx->renderer, scaleX, scaleY);
#endif
    ctx->width = drawableW;
    ctx->height = drawableH;

    if (winW != *lastW || winH != *lastH) {
        layout_static_panes(panes, &paneCount);
        *lastW = winW;
        *lastH = winH;
    }

    updateResizeZones(ctx->window, resizeZones, resizeZoneCount);

#if USE_VULKAN
    VkResult frameResult =
        vk_renderer_begin_frame(ctx->renderer, &commandBuffer, &framebuffer, &extent);

    if (frameResult == VK_ERROR_OUT_OF_DATE_KHR) {
        vk_renderer_recreate_swapchain(ctx->renderer, ctx->window);
        if (timerHudActive) {
            ts_stop_timer("Render");
        }
        return;
    } else if (frameResult != VK_SUCCESS) {
        fprintf(stderr, "[Render] vk_renderer_begin_frame failed: %d\n", frameResult);
        if (timerHudActive) {
            ts_stop_timer("Render");
        }
        return;
    }
#endif

    for (int i = 0; i < paneCount; i++) {
        if (panes[i] && panes[i]->render) {
            panes[i]->render(panes[i], panes[i] == getCoreState()->activeMousePane, core);
        }
    }

    if (isRenaming()) {
        renderPopupQueueContents();  // This draws both popup messages and the rename UI
    }


    if (timerHudActive) {
        ts_stop_timer("Render");

        if (ts_settings.hud_enabled) {
            ts_render(ctx->renderer);
        }
    }

#if USE_VULKAN
    VkResult endResult = vk_renderer_end_frame(ctx->renderer, commandBuffer);
    if (endResult == VK_ERROR_OUT_OF_DATE_KHR || endResult == VK_SUBOPTIMAL_KHR) {
        vk_renderer_recreate_swapchain(ctx->renderer, ctx->window);
    } else if (endResult != VK_SUCCESS) {
        fprintf(stderr, "[Render] vk_renderer_end_frame failed: %d\n", endResult);
    }
#else
    SDL_RenderPresent(ctx->renderer);
#endif


}
