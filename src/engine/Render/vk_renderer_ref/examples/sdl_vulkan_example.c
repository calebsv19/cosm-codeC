#include <SDL2/SDL.h>

#include "vk_renderer.h"

static int recreate_renderer(VkRenderer* renderer, SDL_Window* window) {
    VkResult result = vk_renderer_recreate_swapchain(renderer, window);
    if (result != VK_SUCCESS) {
        SDL_Log("Failed to recreate swapchain: %d", result);
        return -1;
    }
    return 0;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Vulkan SDL Example",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          1280,
                                          720,
                                          SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    VkRenderer renderer;
    VkRendererConfig config;
    vk_renderer_config_set_defaults(&config);
    if (vk_renderer_init(&renderer, window, &config) != VK_SUCCESS) {
        SDL_Log("Failed to initialise Vulkan renderer");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    int running = 1;
    while (running) {
        SDL_Event evt;
        while (SDL_PollEvent(&evt)) {
            if (evt.type == SDL_QUIT) {
                running = 0;
            } else if (evt.type == SDL_WINDOWEVENT &&
                       (evt.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                        evt.window.event == SDL_WINDOWEVENT_RESIZED)) {
                recreate_renderer(&renderer, window);
            }
        }

        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        VkExtent2D extent = {0, 0};
        VkResult frame_result =
            vk_renderer_begin_frame(&renderer, &cmd, &framebuffer, &extent);

        if (frame_result == VK_ERROR_OUT_OF_DATE_KHR) {
            if (recreate_renderer(&renderer, window) != 0) break;
            continue;
        } else if (frame_result != VK_SUCCESS) {
            SDL_Log("vk_renderer_begin_frame failed: %d", frame_result);
            break;
        }

        SDL_Rect rect = {
            .x = (int)(extent.width / 4),
            .y = (int)(extent.height / 4),
            .w = (int)(extent.width / 2),
            .h = (int)(extent.height / 2),
        };

        vk_renderer_set_draw_color(&renderer, 0.15f, 0.35f, 0.85f, 1.0f);
        vk_renderer_fill_rect(&renderer, &rect);
        vk_renderer_set_draw_color(&renderer, 1.0f, 1.0f, 1.0f, 1.0f);
        vk_renderer_draw_line(&renderer, 0.0f, 0.0f, (float)extent.width,
                              (float)extent.height);

        frame_result = vk_renderer_end_frame(&renderer, cmd);
        if (frame_result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreate_renderer(&renderer, window);
        } else if (frame_result != VK_SUCCESS) {
            SDL_Log("vk_renderer_end_frame failed: %d", frame_result);
            break;
        }
    }

    vk_renderer_shutdown(&renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
