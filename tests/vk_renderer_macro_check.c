#include "build_config.h"

#if !USE_VULKAN
#error "vk_renderer_macro_check requires USE_VULKAN=1"
#endif

#include <SDL2/SDL.h>
#include "engine/Render/render_pipeline.h"

#ifndef SDL_RenderFillRect
#error "SDL_RenderFillRect macro not defined; Vulkan compatibility layer inactive."
#endif

#ifndef SDL_RenderDrawRect
#error "SDL_RenderDrawRect macro not defined; Vulkan compatibility layer inactive."
#endif

#ifndef SDL_RenderDrawLine
#error "SDL_RenderDrawLine macro not defined; Vulkan compatibility layer inactive."
#endif

#ifndef SDL_RenderSetClipRect
#error "SDL_RenderSetClipRect macro not defined; Vulkan compatibility layer inactive."
#endif

#ifndef SDL_RenderGetClipRect
#error "SDL_RenderGetClipRect macro not defined; Vulkan compatibility layer inactive."
#endif

#ifndef SDL_RenderIsClipEnabled
#error "SDL_RenderIsClipEnabled macro not defined; Vulkan compatibility layer inactive."
#endif

static void verify_macro_mappings(SDL_Renderer* renderer) {
    SDL_SetRenderDrawColor(renderer, 10, 20, 30, 255);

    SDL_Rect rect = {0, 0, 10, 10};
    SDL_RenderFillRect(renderer, &rect);
    SDL_RenderDrawRect(renderer, &rect);
    SDL_RenderDrawLine(renderer, rect.x, rect.y, rect.x + rect.w, rect.y + rect.h);

    SDL_RenderSetClipRect(renderer, &rect);
    (void)SDL_RenderIsClipEnabled(renderer);
    SDL_Rect out = {0, 0, 0, 0};
    SDL_RenderGetClipRect(renderer, &out);
}

/* Compile-only test: the function is never executed, but the translation unit will
   fail to build if the SDL compat macros are not remapped to the Vulkan renderer. */
static void (*volatile vk_macro_check_ref)(SDL_Renderer*)
#if defined(__clang__) || defined(__GNUC__)
    __attribute__((unused))
#endif
    = verify_macro_mappings;
