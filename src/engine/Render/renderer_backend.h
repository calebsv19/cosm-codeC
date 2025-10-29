#pragma once

#include "build_config.h"
#include <SDL2/SDL.h>

#if USE_VULKAN
#ifndef VK_RENDERER_ENABLE_SDL_COMPAT
#define VK_RENDERER_ENABLE_SDL_COMPAT
#endif

#ifndef VK_RENDERER_SHADER_ROOT
#define VK_RENDERER_SHADER_ROOT "src/engine/Render/vk_renderer_ref"
#endif

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wtypedef-redefinition"
#endif

#include "engine/Render/vk_renderer_ref/src/vk_renderer.h"

#if defined(__clang__)
#pragma clang diagnostic pop
#endif

#include "engine/Render/vk_renderer_ref/src/vk_renderer_compat_sdl.h"

#ifndef SDL_RenderFillRect
#error "Vulkan SDL compatibility layer not active: SDL_RenderFillRect macro missing."
#endif

typedef char SDL_Vulkan_Renderer_Alias_Check[
    (sizeof(VkRenderer*) == sizeof(SDL_Renderer*)) ? 1 : -1];
#endif
