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
#include "ide/Panes/ToolPanels/Project/tool_project.h"


// External dependencies
#include "app/GlobalInfo/core_state.h"

#include "ide/Panes/PaneInfo/pane.h"
#include "ide/UI/layout.h"
#include "ide/Panes/Editor/editor_view.h"
#include "ide/Panes/MenuBar/menu_buttons.h"

#include "core/TextSelection/text_selection_manager.h"


// TimerHud extension
#include "engine/Render/timer_hud_adapter.h"
#include "timer_hud/time_scope.h"


#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>

#if !defined(VK_RENDERER_DEBUG_ENABLED)
#if defined(VK_RENDERER_DEBUG) && VK_RENDERER_DEBUG
#define VK_RENDERER_DEBUG_ENABLED 1
#else
#define VK_RENDERER_DEBUG_ENABLED 0
#endif
#endif

#if !defined(VK_RENDERER_FRAME_DEBUG_ENABLED)
#if defined(VK_RENDERER_FRAME_DEBUG) && VK_RENDERER_FRAME_DEBUG && VK_RENDERER_DEBUG_ENABLED
#define VK_RENDERER_FRAME_DEBUG_ENABLED 1
#else
#define VK_RENDERER_FRAME_DEBUG_ENABLED 0
#endif
#endif

#if !defined(VK_RENDERER_DEBUG_LOG)
#if VK_RENDERER_DEBUG_ENABLED
#define VK_RENDERER_DEBUG_LOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define VK_RENDERER_DEBUG_LOG(...)
#endif
#endif

#if !defined(VK_RENDERER_FRAME_DEBUG_LOG)
#if VK_RENDERER_FRAME_DEBUG_ENABLED
#define VK_RENDERER_FRAME_DEBUG_LOG(...) fprintf(stderr, __VA_ARGS__)
#else
#define VK_RENDERER_FRAME_DEBUG_LOG(...)
#endif
#endif

#if USE_VULKAN
static int s_logged_begin_out_of_date = 0;
static int s_logged_begin_failure = 0;
static int s_logged_end_failure = 0;
static int s_logged_no_draw = 0;
static int s_logged_extent_mismatch = 0;
#if VK_RENDERER_FRAME_DEBUG_ENABLED
static unsigned s_debug_frame_counter = 0;
#endif
#endif


RenderContext globalRenderContext;

RenderContext* getRenderContext() {
    return &globalRenderContext;
}

void setRenderContext(VkRenderer* renderer, SDL_Window* window,
                      int width, int height) {
    globalRenderContext.renderer = renderer;
    globalRenderContext.window = window;
    globalRenderContext.width = width;
    globalRenderContext.height = height;

#if USE_VULKAN && VK_RENDERER_DEBUG_ENABLED
    if (renderer) {
        SDL_Log("[RenderContext] Vulkan renderer set (renderer=%p stored=%p window=%p swapchain images=%u extent=%ux%u format=%u)",
                (void*)renderer,
                (void*)globalRenderContext.renderer,
                (void*)window,
                renderer->context.swapchain.image_count,
                renderer->context.swapchain.extent.width,
                renderer->context.swapchain.extent.height,
                renderer->context.swapchain.image_format);
    } else {
        SDL_Log("[RenderContext] Vulkan renderer set to NULL");
    }
#endif
}




// ========================================
//   SECTION: Initialization


bool initRenderPipeline() {
    if (getCoreState()->timerHudEnabled) {
        timer_hud_register_backend();
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
    text_selection_manager_begin_frame();
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
    /* Keep Vulkan logical coordinates aligned with window space; Vulkan backend
       will handle scaling to the swapchain extent. */
    vk_renderer_set_logical_size(ctx->renderer, (float)winW, (float)winH);

    if (ctx->renderer && drawableW > 0 && drawableH > 0) {
        VkExtent2D swapExtent = ctx->renderer->context.swapchain.extent;
        if (swapExtent.width != (uint32_t)drawableW ||
            swapExtent.height != (uint32_t)drawableH) {
            if (!s_logged_extent_mismatch) {
                fprintf(stderr,
                        "[Render] Drawable size (%dx%d) differs from swapchain extent (%ux%u); "
                        "forcing swapchain recreate.\n",
                        drawableW, drawableH,
                        swapExtent.width, swapExtent.height);
                s_logged_extent_mismatch = 1;
            }
            vk_renderer_recreate_swapchain(ctx->renderer, ctx->window);
            s_logged_begin_out_of_date = 0;
            s_logged_begin_failure = 0;
            s_logged_end_failure = 0;
            if (timerHudActive) {
                ts_stop_timer("Render");
            }
            return;
        }
        s_logged_extent_mismatch = 0;
    }
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

    if (frameResult == VK_ERROR_OUT_OF_DATE_KHR ||
        frameResult == VK_SUBOPTIMAL_KHR) {
        if (!s_logged_begin_out_of_date) {
            fprintf(stderr,
                    "[Render] vk_renderer_begin_frame reported %s (win=%dx%d drawable=%dx%d); "
                    "triggering swapchain recreate.\n",
                    (frameResult == VK_SUBOPTIMAL_KHR) ? "SUBOPTIMAL" : "OUT_OF_DATE",
                    winW, winH, drawableW, drawableH);
            s_logged_begin_out_of_date = 1;
        }
        vk_renderer_recreate_swapchain(ctx->renderer, ctx->window);
        s_logged_begin_failure = 0;
        s_logged_end_failure = 0;
        if (timerHudActive) {
            ts_stop_timer("Render");
        }
        return;
    } else if (frameResult != VK_SUCCESS) {
        if (!s_logged_begin_failure) {
            fprintf(stderr, "[Render] vk_renderer_begin_frame failed: %d\n", frameResult);
            s_logged_begin_failure = 1;
        }
        if (timerHudActive) {
            ts_stop_timer("Render");
        }
        return;
    }
    s_logged_begin_out_of_date = 0;
    s_logged_begin_failure = 0;
#endif

    for (int i = 0; i < paneCount; i++) {
        if (panes[i] && panes[i]->render) {
            panes[i]->render(panes[i], panes[i] == getCoreState()->activeMousePane, core);
        }
    }
    if (isRenaming()) {
        renderPopupQueueContents();  // This draws both popup messages and the rename UI
    }
    renderProjectDragOverlay();
#if USE_VULKAN && VK_RENDERER_FRAME_DEBUG_ENABLED
    if (s_debug_frame_counter < 120 && ctx && ctx->renderer) {
        VK_RENDERER_FRAME_DEBUG_LOG(
            "[Render] Frame %u paneCount=%d drawCalls=%u window=%dx%d drawable=%dx%d\n",
            s_debug_frame_counter,
            paneCount,
            (unsigned)ctx->renderer->draw_state.draw_call_count,
            winW, winH,
            drawableW, drawableH);
    }
#endif

        if (timerHudActive) {
        ts_stop_timer("Render");

        if (ts_settings.hud_enabled) {
            ts_render();
        }
    }

#if USE_VULKAN
    if (ctx->renderer) {
        uint32_t draw_calls = ctx->renderer->draw_state.draw_call_count;
        if (draw_calls == 0) {
            if (!s_logged_no_draw) {
                VK_RENDERER_FRAME_DEBUG_LOG(
                    "[Render] No Vulkan draw calls recorded this frame (paneCount=%d).\n",
                    paneCount);
                s_logged_no_draw = 1;
            }
        } else if (s_logged_no_draw) {
            VK_RENDERER_FRAME_DEBUG_LOG(
                "[Render] Vulkan draw calls resumed (%u calls this frame).\n",
                (unsigned)draw_calls);
            s_logged_no_draw = 0;
        }
    }

    VkResult endResult = vk_renderer_end_frame(ctx->renderer, commandBuffer);
    if (endResult == VK_ERROR_OUT_OF_DATE_KHR || endResult == VK_SUBOPTIMAL_KHR) {
        vk_renderer_recreate_swapchain(ctx->renderer, ctx->window);
        s_logged_end_failure = 0;
    } else if (endResult != VK_SUCCESS) {
        if (!s_logged_end_failure) {
            fprintf(stderr, "[Render] vk_renderer_end_frame failed: %d\n", endResult);
            s_logged_end_failure = 1;
        }
    } else {
        s_logged_end_failure = 0;
    }
#else
    SDL_RenderPresent(ctx->renderer);
#endif

#if USE_VULKAN && VK_RENDERER_FRAME_DEBUG_ENABLED
    if (s_debug_frame_counter < 120) {
        s_debug_frame_counter++;
    }
#endif


}
