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
#include "ide/UI/layout_config.h"
#include "ide/UI/ui_state.h"
#include "ide/UI/shared_theme_font_adapter.h"
#include "ide/Panes/Editor/editor_view.h"
#include "ide/Panes/MenuBar/menu_buttons.h"

#include "core/TextSelection/text_selection_manager.h"


// TimerHud extension
#include "engine/Render/timer_hud_adapter.h"
#include "timer_hud/time_scope.h"


#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
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
    if (globalRenderContext.renderer && globalRenderContext.renderer != renderer) {
        render_text_cache_shutdown();
    }
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
        const char* respectSettings = getenv("IDE_TIMER_HUD_RESPECT_SETTINGS");
        const bool useSettings = (respectSettings && respectSettings[0] &&
                                  (strcmp(respectSettings, "1") == 0 ||
                                   strcasecmp(respectSettings, "true") == 0));
        if (!useSettings) {
            ts_settings.hud_enabled = true;
        }
        const char* overlayEnv = getenv("IDE_TIMER_HUD_OVERLAY");
        int overlayEnabled = ts_settings.hud_enabled ? 1 : 0;
        if (overlayEnv && overlayEnv[0]) {
            if (strcmp(overlayEnv, "0") == 0 || strcasecmp(overlayEnv, "false") == 0 ||
                strcasecmp(overlayEnv, "off") == 0 || strcasecmp(overlayEnv, "no") == 0) {
                ts_settings.hud_enabled = false;
                overlayEnabled = 0;
            } else if (strcmp(overlayEnv, "1") == 0 || strcasecmp(overlayEnv, "true") == 0 ||
                       strcasecmp(overlayEnv, "on") == 0 || strcasecmp(overlayEnv, "yes") == 0) {
                ts_settings.hud_enabled = true;
                overlayEnabled = 1;
            }
        }

        // Optional override: IDE_TIMER_HUD_VISUAL_MODE=text_compact|history_graph|hybrid.
        const char* visualModeEnv = getenv("IDE_TIMER_HUD_VISUAL_MODE");
        if (visualModeEnv && visualModeEnv[0]) {
            ts_set_hud_visual_mode(visualModeEnv);
        } else if (overlayEnabled) {
            // Overlay-focused runs default to hybrid (compact text + history line).
            ts_set_hud_visual_mode("hybrid");
        }
        fprintf(stderr,
                "[TimerHUD] initialized (hud_enabled=%d mode=%s log_enabled=%d log_file=%s)\n",
                ts_settings.hud_enabled ? 1 : 0,
                ts_settings.hud_visual_mode,
                ts_settings.log_enabled ? 1 : 0,
                ts_settings.log_filepath);
    }
    return initFontSystem();
}

void shutdownRenderPipeline() {
    if (getCoreState()->timerHudEnabled) {
        ts_shutdown();
    }
    render_text_cache_shutdown();
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
    (void)hovered;

    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer* renderer = ctx->renderer;

    SDL_SetRenderDrawColor(renderer,
        pane->bgColor.r, pane->bgColor.g, pane->bgColor.b, pane->bgColor.a);

    SDL_Rect rect = { pane->x, pane->y, pane->w, pane->h };
    SDL_RenderFillRect(renderer, &rect);

}

static void render_pane_borders(UIPane** panes, int paneCount, int winW, int winH) {
    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer || !panes || paneCount <= 0) return;
    SDL_Renderer* renderer = ctx->renderer;
    SDL_RenderSetClipRect(renderer, NULL);

    for (int i = 0; i < paneCount; ++i) {
        UIPane* pane = panes[i];
        if (!pane || !pane->visible || pane->w <= 0 || pane->h <= 0) continue;

        SDL_Rect rect = { pane->x, pane->y, pane->w, pane->h };
        const int left = rect.x;
        const int right = rect.x + rect.w - 1;
        const int top = rect.y;
        const int bottom = rect.y + rect.h - 1;

        SDL_Color borderColor = pane->borderColor;
        SDL_SetRenderDrawColor(renderer, borderColor.r, borderColor.g, borderColor.b, borderColor.a);

        // Single-owner seam policy:
        // top and left own shared boundaries; right/bottom are skipped to avoid double-thick seams.
        if (left > 0) {
            SDL_RenderDrawLine(renderer, left, top, left, bottom);
        }
        if (top > 0) {
            SDL_RenderDrawLine(renderer, left, top, right, top);
        }

        (void)winW;
        (void)winH;
    }
}

static void render_hovered_pane_overlay(const IDECoreState* core) {
    if (!core || !core->activeMousePane) return;
    UIPane* hoveredPane = core->activeMousePane;
    if (!hoveredPane->visible) return;
    if (hoveredPane->role == PANE_ROLE_EDITOR) return;

    RenderContext* ctx = getRenderContext();
    if (!ctx || !ctx->renderer) return;
    SDL_Renderer* renderer = ctx->renderer;

    SDL_RenderSetClipRect(renderer, NULL);
    SDL_Rect rect = {
        hoveredPane->x + 1,
        hoveredPane->y + 1,
        hoveredPane->w - 2,
        hoveredPane->h - 2
    };
    if (rect.w <= 0 || rect.h <= 0) return;
    SDL_Color hoverColor = ide_shared_theme_pane_hover_border_color();
    SDL_SetRenderDrawColor(renderer, hoverColor.r, hoverColor.g, hoverColor.b, hoverColor.a);
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

    if (timerHudActive) {
        ts_start_timer("WindowAndScale");
    }
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
    if (timerHudActive) {
        ts_stop_timer("WindowAndScale");
    }
    ctx->width = drawableW;
    ctx->height = drawableH;

    LayoutDimensions* dims = getLayoutDimensions();
    UIState* ui = getUIState();
    const int toolWidth = dims ? dims->toolWidth : 0;
    const int controlWidth = dims ? dims->controlWidth : 0;
    const int terminalHeight = dims ? dims->terminalHeight : 0;
    const int toolVisible = (ui && ui->toolPanelVisible) ? 1 : 0;
    const int controlVisible = (ui && ui->controlPanelVisible) ? 1 : 0;

    static int s_prev_win_w = -1;
    static int s_prev_win_h = -1;
    static int s_prev_tool_w = -1;
    static int s_prev_control_w = -1;
    static int s_prev_terminal_h = -1;
    static int s_prev_tool_visible = -1;
    static int s_prev_control_visible = -1;

    const bool layoutChanged =
        (winW != s_prev_win_w) ||
        (winH != s_prev_win_h) ||
        (toolWidth != s_prev_tool_w) ||
        (controlWidth != s_prev_control_w) ||
        (terminalHeight != s_prev_terminal_h) ||
        (toolVisible != s_prev_tool_visible) ||
        (controlVisible != s_prev_control_visible);

    if (timerHudActive) {
        ts_start_timer("ResizeLayout");
    }
    if (layoutChanged) {
        layout_static_panes(panes, &paneCount);
        *lastW = winW;
        *lastH = winH;
    }
    if (timerHudActive) {
        ts_stop_timer("ResizeLayout");
    }

    if (timerHudActive) {
        ts_start_timer("ResizeZones");
    }
    if (layoutChanged) {
        updateResizeZones(ctx->window, resizeZones, resizeZoneCount);
    }
    if (timerHudActive) {
        ts_stop_timer("ResizeZones");
    }

    s_prev_win_w = winW;
    s_prev_win_h = winH;
    s_prev_tool_w = toolWidth;
    s_prev_control_w = controlWidth;
    s_prev_terminal_h = terminalHeight;
    s_prev_tool_visible = toolVisible;
    s_prev_control_visible = controlVisible;

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

    if (timerHudActive) {
        ts_start_timer("PaneRender");
    }
    for (int i = 0; i < paneCount; i++) {
        UIPane* pane = panes[i];
        if (!pane || !pane->render) continue;

        // Keep cache dependency bookkeeping in sync with pane geometry.
        if (pane->cacheWidth != pane->w || pane->cacheHeight != pane->h) {
            paneMarkDirty(pane, PANE_INVALIDATION_LAYOUT | PANE_INVALIDATION_RESIZE);
        }

        if (pane->dirty || (core && core->fullRedrawRequired)) {
            pane->cacheValid = false;
        }

        pane->render(pane, pane == getCoreState()->activeMousePane, core);

        if (!pane->dirty) {
            pane->cacheValid = true;
        }
    }
    if (timerHudActive) {
        ts_stop_timer("PaneRender");
    }

    render_pane_borders(panes, paneCount, winW, winH);

    if (timerHudActive) {
        ts_start_timer("OverlayRender");
    }
    if (isRenaming()) {
        renderPopupQueueContents();  // This draws both popup messages and the rename UI
    }
    renderProjectDragOverlay();
    if (timerHudActive) {
        ts_stop_timer("OverlayRender");
    }

    render_hovered_pane_overlay(core);

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
        ts_start_timer("HudRender");
    }
    if (timerHudActive) {
        timer_hud_bind_renderer((SDL_Renderer*)ctx->renderer);
        SDL_RenderSetClipRect(ctx->renderer, NULL);
        ts_stop_timer("Render");

        if (ts_settings.hud_enabled) {
            ts_render();
        }
    }
    if (timerHudActive) {
        ts_stop_timer("HudRender");
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

    if (timerHudActive) {
        ts_start_timer("Present");
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
    if (timerHudActive) {
        ts_stop_timer("Present");
    }
#else
    if (timerHudActive) {
        ts_start_timer("Present");
    }
    SDL_RenderPresent(ctx->renderer);
    if (timerHudActive) {
        ts_stop_timer("Present");
    }
#endif

#if USE_VULKAN && VK_RENDERER_FRAME_DEBUG_ENABLED
    if (s_debug_frame_counter < 120) {
        s_debug_frame_counter++;
    }
#endif


}
