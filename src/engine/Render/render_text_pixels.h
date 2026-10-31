#ifndef IDE_RENDER_TEXT_PIXELS_H
#define IDE_RENDER_TEXT_PIXELS_H
#include "engine/Render/renderer_backend.h"
#include <math.h>
/* Frame-space coordinates; fonts already include user text zoom. */
static inline void ide_text_pixel_scale(SDL_Renderer* renderer, float* sx, float* sy) {
    *sx=*sy=1;
#if USE_VULKAN
    VkRenderer* vk=(VkRenderer*)renderer;
    if (vk) {
        if (vk->draw_state.logical_size[0]>0) *sx=vk->context.swapchain.extent.width/vk->draw_state.logical_size[0];
        if (vk->draw_state.logical_size[1]>0) *sy=vk->context.swapchain.extent.height/vk->draw_state.logical_size[1];
    }
#else
    (void)renderer;
#endif
    if (!isfinite(*sx) || *sx<=0) *sx=1;
    if (!isfinite(*sy) || *sy<=0) *sy=1;
}
#if USE_VULKAN
void ide_text_draw_native_pixels(VkRenderer* renderer, const VkRendererTexture* texture,
                                int x, int y, bool bold, const SDL_Rect* clip);
#endif
#endif
