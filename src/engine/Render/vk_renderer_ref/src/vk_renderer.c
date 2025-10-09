#include "vk_renderer.h"

#include <SDL2/SDL_surface.h>
#include <SDL2/SDL_vulkan.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VK_RENDERER_VERTEX_BUFFER_SIZE (256 * 1024)

static VkResult create_descriptor_resources(VkRenderer* renderer) {
    VkDescriptorSetLayoutBinding sampler_binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        .pImmutableSamplers = NULL,
    };

    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &sampler_binding,
    };

    VkResult result = vkCreateDescriptorSetLayout(renderer->context.device, &layout_info, NULL,
                                                  &renderer->sampler_set_layout);
    if (result != VK_SUCCESS) return result;

    VkDescriptorPoolSize pool_size = {
        .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 128,
    };

    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 128,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
    };

    return vkCreateDescriptorPool(renderer->context.device, &pool_info, NULL,
                                  &renderer->descriptor_pool);
}

static void flush_transient_textures(VkRenderer* renderer, VkRendererFrameState* frame) {
    if (!frame || !frame->transient_textures) {
        frame->transient_texture_count = 0;
        return;
    }
    for (uint32_t i = 0; i < frame->transient_texture_count; ++i) {
        vk_renderer_texture_destroy(renderer, &frame->transient_textures[i]);
    }
    frame->transient_texture_count = 0;
}

static VkResult create_render_pass(VkRenderer* renderer) {
    VkAttachmentDescription color_attachment = {
        .format = renderer->context.swapchain.image_format,
        .samples = renderer->config.msaa_samples,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference color_reference = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_reference,
    };

    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    return vkCreateRenderPass(renderer->context.device, &render_pass_info, NULL,
                              &renderer->render_pass);
}

static void destroy_framebuffers(VkRenderer* renderer) {
    if (!renderer->swapchain_framebuffers) return;
    for (uint32_t i = 0; i < renderer->swapchain_framebuffer_count; ++i) {
        if (renderer->swapchain_framebuffers[i]) {
            vkDestroyFramebuffer(renderer->context.device,
                                 renderer->swapchain_framebuffers[i], NULL);
        }
    }
    free(renderer->swapchain_framebuffers);
    renderer->swapchain_framebuffers = NULL;
    renderer->swapchain_framebuffer_count = 0;
}

static VkResult create_framebuffers(VkRenderer* renderer) {
    destroy_framebuffers(renderer);

    uint32_t count = renderer->context.swapchain.image_count;
    renderer->swapchain_framebuffers =
        (VkFramebuffer*)calloc(count, sizeof(VkFramebuffer));
    if (!renderer->swapchain_framebuffers) return VK_ERROR_OUT_OF_HOST_MEMORY;

    for (uint32_t i = 0; i < count; ++i) {
        VkImageView attachments[] = {renderer->context.swapchain.image_views[i]};

        VkFramebufferCreateInfo framebuffer_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = renderer->render_pass,
            .attachmentCount = 1,
            .pAttachments = attachments,
            .width = renderer->context.swapchain.extent.width,
            .height = renderer->context.swapchain.extent.height,
            .layers = 1,
        };

        VkResult result = vkCreateFramebuffer(renderer->context.device, &framebuffer_info, NULL,
                                              &renderer->swapchain_framebuffers[i]);
        if (result != VK_SUCCESS) return result;
    }

    renderer->swapchain_framebuffer_count = count;
    return VK_SUCCESS;
}

static VkResult create_pipeline_cache(VkRenderer* renderer) {
    VkPipelineCacheCreateInfo cache_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
    };
    return vkCreatePipelineCache(renderer->context.device, &cache_info, NULL,
                                 &renderer->pipeline_cache);
}

static VkResult ensure_frame_vertex_buffer(VkRenderer* renderer,
                                           VkRendererFrameState* frame,
                                           VkDeviceSize required) {
    if (frame->vertex_buffer.buffer != VK_NULL_HANDLE &&
        frame->vertex_offset + required <= frame->vertex_buffer.size) {
        return VK_SUCCESS;
    }

    VkDeviceSize new_size = VK_RENDERER_VERTEX_BUFFER_SIZE;
    while (new_size < required) new_size *= 2;

    VkDeviceSize previous_offset = frame->vertex_offset;

    VkAllocatedBuffer new_buffer = {0};
    VkResult result = vk_renderer_memory_create_buffer(
        &renderer->context, new_size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &new_buffer);
    if (result != VK_SUCCESS) {
        return result;
    }

    if (frame->vertex_buffer.buffer != VK_NULL_HANDLE && frame->vertex_offset > 0) {
        memcpy(new_buffer.mapped, frame->vertex_buffer.mapped, (size_t)frame->vertex_offset);
        vk_renderer_memory_destroy_buffer(&renderer->context, &frame->vertex_buffer);
    }

    frame->vertex_buffer = new_buffer;
    frame->vertex_offset = previous_offset;
    return VK_SUCCESS;
}

static void push_basic_constants(VkRenderer* renderer,
                                 VkCommandBuffer cmd,
                                 VkRendererPipelineKind kind) {
    float viewport_data[4] = {
        (float)renderer->context.swapchain.extent.width,
        (float)renderer->context.swapchain.extent.height,
        0.0f,
        0.0f,
    };

    float color_data[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    vkCmdPushConstants(cmd, renderer->pipelines[kind].layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(viewport_data), viewport_data);
    vkCmdPushConstants(cmd, renderer->pipelines[kind].layout,
                       VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(viewport_data),
                       sizeof(color_data), color_data);
}

VkResult vk_renderer_init(VkRenderer* renderer,
                          SDL_Window* window,
                          const VkRendererConfig* config) {
    if (!renderer || !window) return VK_ERROR_INITIALIZATION_FAILED;
    memset(renderer, 0, sizeof(*renderer));
    renderer->current_frame_index = UINT32_MAX;

    VkRendererConfig local_config;
    if (config) {
        local_config = *config;
    } else {
        vk_renderer_config_set_defaults(&local_config);
    }
    renderer->config = local_config;

    VkResult result =
        vk_renderer_context_create(&renderer->context, window, &renderer->config);
    if (result != VK_SUCCESS) {
        vk_renderer_shutdown(renderer);
        return result;
    }

    result = create_descriptor_resources(renderer);
    if (result != VK_SUCCESS) {
        vk_renderer_shutdown(renderer);
        return result;
    }

    result = create_render_pass(renderer);
    if (result != VK_SUCCESS) {
        vk_renderer_shutdown(renderer);
        return result;
    }

    result = create_pipeline_cache(renderer);
    if (result != VK_SUCCESS) {
        vk_renderer_shutdown(renderer);
        return result;
    }

    result = vk_renderer_pipeline_create_all(&renderer->context, renderer->render_pass,
                                             renderer->sampler_set_layout, renderer->pipeline_cache,
                                             renderer->pipelines);
    if (result != VK_SUCCESS) {
        vk_renderer_shutdown(renderer);
        return result;
    }

    result = vk_renderer_commands_init(renderer, &renderer->command_pool,
                                       renderer->config.frames_in_flight);
    if (result != VK_SUCCESS) {
        vk_renderer_shutdown(renderer);
        return result;
    }

    for (uint32_t i = 0; i < renderer->frame_count; ++i) {
        result = ensure_frame_vertex_buffer(renderer, &renderer->frames[i],
                                            VK_RENDERER_VERTEX_BUFFER_SIZE);
        if (result != VK_SUCCESS) {
            vk_renderer_shutdown(renderer);
            return result;
        }
    }

    result = create_framebuffers(renderer);
    if (result != VK_SUCCESS) {
        vk_renderer_shutdown(renderer);
        return result;
    }

    renderer->draw_state.current_color[0] = 1.0f;
    renderer->draw_state.current_color[1] = 1.0f;
    renderer->draw_state.current_color[2] = 1.0f;
    renderer->draw_state.current_color[3] = 1.0f;

    return VK_SUCCESS;
}

static VkRendererFrameState* active_frame(VkRenderer* renderer) {
    if (!renderer || renderer->current_frame_index >= renderer->frame_count) return NULL;
    return &renderer->frames[renderer->current_frame_index];
}

VkResult vk_renderer_begin_frame(VkRenderer* renderer,
                                 VkCommandBuffer* out_cmd,
                                 VkFramebuffer* out_framebuffer,
                                 VkExtent2D* out_extent) {
    if (!renderer || !out_cmd || !out_framebuffer) return VK_ERROR_INITIALIZATION_FAILED;

    uint32_t frame_index = 0;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkResult result = vk_renderer_commands_begin_frame(renderer, &frame_index, &cmd);
    if (result != VK_SUCCESS) return result;

    renderer->current_frame_index = frame_index;

    VkRendererFrameState* frame = active_frame(renderer);
    if (!frame) return VK_ERROR_INITIALIZATION_FAILED;
    flush_transient_textures(renderer, frame);
    frame->vertex_offset = 0;

    VkClearValue clear = {
        .color = {
            .float32 = {renderer->config.clear_color[0], renderer->config.clear_color[1],
                        renderer->config.clear_color[2], renderer->config.clear_color[3]},
        },
    };

    VkFramebuffer framebuffer =
        renderer->swapchain_framebuffers[renderer->swapchain_image_index];

    VkRenderPassBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderer->render_pass,
        .framebuffer = framebuffer,
        .renderArea = {
            .offset = {0, 0},
            .extent = renderer->context.swapchain.extent,
        },
        .clearValueCount = 1,
        .pClearValues = &clear,
    };

    vkCmdBeginRenderPass(cmd, &begin_info, VK_SUBPASS_CONTENTS_INLINE);

    *out_cmd = cmd;
    *out_framebuffer = framebuffer;
    if (out_extent) *out_extent = renderer->context.swapchain.extent;
    return VK_SUCCESS;
}

VkResult vk_renderer_end_frame(VkRenderer* renderer,
                               VkCommandBuffer cmd) {
    if (!renderer) return VK_ERROR_INITIALIZATION_FAILED;
    VkRendererFrameState* frame = active_frame(renderer);
    if (!frame) return VK_ERROR_INITIALIZATION_FAILED;

    vkCmdEndRenderPass(cmd);

    VkResult result =
        vk_renderer_commands_end_frame(renderer, renderer->current_frame_index, cmd);
    renderer->current_frame_index = UINT32_MAX;
    return result;
}

VkResult vk_renderer_recreate_swapchain(VkRenderer* renderer, SDL_Window* window) {
    if (!renderer || !window) return VK_ERROR_INITIALIZATION_FAILED;
    VkDevice device = renderer->context.device;
    vkDeviceWaitIdle(device);

    destroy_framebuffers(renderer);
    vk_renderer_pipeline_destroy_all(&renderer->context, renderer->pipelines);

    VkResult result =
        vk_renderer_context_recreate_swapchain(&renderer->context, window, &renderer->config);
    if (result != VK_SUCCESS) return result;

    result = vk_renderer_pipeline_create_all(&renderer->context, renderer->render_pass,
                                             renderer->sampler_set_layout, renderer->pipeline_cache,
                                             renderer->pipelines);
    if (result != VK_SUCCESS) return result;

    return create_framebuffers(renderer);
}

void vk_renderer_set_draw_color(VkRenderer* renderer, float r, float g, float b, float a) {
    if (!renderer) return;
    renderer->draw_state.current_color[0] = r;
    renderer->draw_state.current_color[1] = g;
    renderer->draw_state.current_color[2] = b;
    renderer->draw_state.current_color[3] = a;
}

void vk_renderer_draw_line(VkRenderer* renderer,
                           float x0,
                           float y0,
                           float x1,
                           float y1) {
    VkRendererFrameState* frame = active_frame(renderer);
    if (!frame) return;

    const float vertices[] = {
        x0, y0, renderer->draw_state.current_color[0], renderer->draw_state.current_color[1],
        renderer->draw_state.current_color[2], renderer->draw_state.current_color[3],
        x1, y1, renderer->draw_state.current_color[0], renderer->draw_state.current_color[1],
        renderer->draw_state.current_color[2], renderer->draw_state.current_color[3],
    };

    VkDeviceSize bytes = sizeof(vertices);
    if (ensure_frame_vertex_buffer(renderer, frame, bytes) != VK_SUCCESS) return;

    uint8_t* dst = (uint8_t*)frame->vertex_buffer.mapped + frame->vertex_offset;
    memcpy(dst, vertices, bytes);

    VkDeviceSize offset = frame->vertex_offset;
    frame->vertex_offset += bytes;

    VkBuffer buffers[] = {frame->vertex_buffer.buffer};
    VkDeviceSize offsets[] = {offset};
    VkCommandBuffer cmd = frame->command_buffer;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      renderer->pipelines[VK_RENDERER_PIPELINE_LINES].pipeline);
    vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
    push_basic_constants(renderer, cmd, VK_RENDERER_PIPELINE_LINES);
    vkCmdDraw(cmd, 2, 1, 0, 0);
}

static void emit_filled_quad(VkRenderer* renderer,
                             VkRendererFrameState* frame,
                             const float quad[6][6],
                             VkRendererPipelineKind pipeline,
                             uint32_t vertex_count) {
    VkDeviceSize bytes = sizeof(float) * 6 * vertex_count;
    if (ensure_frame_vertex_buffer(renderer, frame, bytes) != VK_SUCCESS) return;

    uint8_t* dst = (uint8_t*)frame->vertex_buffer.mapped + frame->vertex_offset;
    memcpy(dst, quad, (size_t)bytes);

    VkDeviceSize offset = frame->vertex_offset;
    frame->vertex_offset += bytes;

    VkBuffer buffers[] = {frame->vertex_buffer.buffer};
    VkDeviceSize offsets[] = {offset};
    VkCommandBuffer cmd = frame->command_buffer;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      renderer->pipelines[pipeline].pipeline);
    vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
    push_basic_constants(renderer, cmd, pipeline);
    vkCmdDraw(cmd, vertex_count, 1, 0, 0);
}

void vk_renderer_draw_rect(VkRenderer* renderer, const SDL_Rect* rect) {
    if (!rect) return;
    float x = (float)rect->x;
    float y = (float)rect->y;
    float w = (float)rect->w;
    float h = (float)rect->h;

    vk_renderer_draw_line(renderer, x, y, x + w, y);
    vk_renderer_draw_line(renderer, x + w, y, x + w, y + h);
    vk_renderer_draw_line(renderer, x + w, y + h, x, y + h);
    vk_renderer_draw_line(renderer, x, y + h, x, y);
}

void vk_renderer_fill_rect(VkRenderer* renderer, const SDL_Rect* rect) {
    VkRendererFrameState* frame = active_frame(renderer);
    if (!frame || !rect) return;

    float x = (float)rect->x;
    float y = (float)rect->y;
    float w = (float)rect->w;
    float h = (float)rect->h;

    const float color[4] = {
        renderer->draw_state.current_color[0],
        renderer->draw_state.current_color[1],
        renderer->draw_state.current_color[2],
        renderer->draw_state.current_color[3],
    };

    float quad[6][6] = {
        {x, y, color[0], color[1], color[2], color[3]},
        {x + w, y, color[0], color[1], color[2], color[3]},
        {x + w, y + h, color[0], color[1], color[2], color[3]},
        {x, y, color[0], color[1], color[2], color[3]},
        {x + w, y + h, color[0], color[1], color[2], color[3]},
        {x, y + h, color[0], color[1], color[2], color[3]},
    };

    emit_filled_quad(renderer, frame, quad, VK_RENDERER_PIPELINE_SOLID, 6);
}

void vk_renderer_draw_texture(VkRenderer* renderer,
                              const VkRendererTexture* texture,
                              const SDL_Rect* src,
                              const SDL_Rect* dst) {
    VkRendererFrameState* frame = active_frame(renderer);
    if (!frame || !texture) return;

    SDL_Rect local_dst;
    if (!dst) {
        local_dst.x = 0;
        local_dst.y = 0;
        local_dst.w = (int)texture->width;
        local_dst.h = (int)texture->height;
        dst = &local_dst;
    }

    float sx = 0.0f;
    float sy = 0.0f;
    float sw = (float)texture->width;
    float sh = (float)texture->height;

    if (src) {
        sx = (float)src->x;
        sy = (float)src->y;
        sw = (float)src->w;
        sh = (float)src->h;
    }

    float u0 = sx / (float)texture->width;
    float v0 = sy / (float)texture->height;
    float u1 = (sx + sw) / (float)texture->width;
    float v1 = (sy + sh) / (float)texture->height;

    float x = (float)dst->x;
    float y = (float)dst->y;
    float w = (float)dst->w;
    float h = (float)dst->h;

    float textured_vertices[6][8] = {
        {x, y, u0, v0, 1.0f, 1.0f, 1.0f, 1.0f},
        {x + w, y, u1, v0, 1.0f, 1.0f, 1.0f, 1.0f},
        {x + w, y + h, u1, v1, 1.0f, 1.0f, 1.0f, 1.0f},
        {x, y, u0, v0, 1.0f, 1.0f, 1.0f, 1.0f},
        {x + w, y + h, u1, v1, 1.0f, 1.0f, 1.0f, 1.0f},
        {x, y + h, u0, v1, 1.0f, 1.0f, 1.0f, 1.0f},
    };

    VkDeviceSize bytes = sizeof(textured_vertices);
    if (ensure_frame_vertex_buffer(renderer, frame, bytes) != VK_SUCCESS) return;

    uint8_t* dst_ptr = (uint8_t*)frame->vertex_buffer.mapped + frame->vertex_offset;
    memcpy(dst_ptr, textured_vertices, bytes);

    VkDeviceSize offset = frame->vertex_offset;
    frame->vertex_offset += bytes;

    VkBuffer buffers[] = {frame->vertex_buffer.buffer};
    VkDeviceSize offsets[] = {offset};
    VkCommandBuffer cmd = frame->command_buffer;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      renderer->pipelines[VK_RENDERER_PIPELINE_TEXTURED].pipeline);
    vkCmdBindVertexBuffers(cmd, 0, 1, buffers, offsets);
    push_basic_constants(renderer, cmd, VK_RENDERER_PIPELINE_TEXTURED);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            renderer->pipelines[VK_RENDERER_PIPELINE_TEXTURED].layout, 0, 1,
                            &texture->descriptor_set, 0, NULL);

    vkCmdDraw(cmd, 6, 1, 0, 0);
}

VkResult vk_renderer_upload_sdl_surface(VkRenderer* renderer,
                                        SDL_Surface* surface,
                                        VkRendererTexture* out_texture) {
    if (!renderer || !surface || !out_texture) return VK_ERROR_INITIALIZATION_FAILED;

    SDL_Surface* converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ABGR8888, 0);
    SDL_Surface* used_surface = converted ? converted : surface;

    VkResult result = vk_renderer_texture_create_from_rgba(
        renderer, used_surface->pixels, (uint32_t)used_surface->w, (uint32_t)used_surface->h,
        out_texture);

    if (converted) SDL_FreeSurface(converted);
    return result;
}

void vk_renderer_queue_texture_destroy(VkRenderer* renderer,
                                       VkRendererTexture* texture) {
    if (!renderer || !texture) return;
    if (!texture->descriptor_set && !texture->sampler && !texture->image.image) {
        return;
    }

    uint32_t frame_index = renderer->current_frame_index;
    if (frame_index == UINT32_MAX) {
        vk_renderer_texture_destroy(renderer, texture);
        return;
    }

    VkRendererFrameState* frame = &renderer->frames[frame_index];
    if (!frame) {
        vk_renderer_texture_destroy(renderer, texture);
        return;
    }

    if (frame->transient_texture_count >= frame->transient_texture_capacity) {
        uint32_t new_capacity = frame->transient_texture_capacity ? frame->transient_texture_capacity * 2 : 8;
        VkRendererTexture* resized = (VkRendererTexture*)realloc(frame->transient_textures,
                                                                 sizeof(VkRendererTexture) * new_capacity);
        if (!resized) {
            vk_renderer_texture_destroy(renderer, texture);
            return;
        }
        frame->transient_textures = resized;
        frame->transient_texture_capacity = new_capacity;
    }

    frame->transient_textures[frame->transient_texture_count++] = *texture;
    memset(texture, 0, sizeof(*texture));
}

void vk_renderer_shutdown(VkRenderer* renderer) {
    if (!renderer) return;
    VkDevice device = renderer->context.device;
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
    }

    if (renderer->frames) {
        for (uint32_t i = 0; i < renderer->frame_count; ++i) {
            VkRendererFrameState* frame = &renderer->frames[i];
            flush_transient_textures(renderer, frame);
            free(frame->transient_textures);
            frame->transient_textures = NULL;
            frame->transient_texture_capacity = 0;
            frame->transient_texture_count = 0;
            vk_renderer_memory_destroy_buffer(&renderer->context, &frame->vertex_buffer);
        }
    }

    destroy_framebuffers(renderer);

    if (device != VK_NULL_HANDLE) {
        if (renderer->descriptor_pool) {
            vkDestroyDescriptorPool(device, renderer->descriptor_pool, NULL);
            renderer->descriptor_pool = VK_NULL_HANDLE;
        }

        if (renderer->sampler_set_layout) {
            vkDestroyDescriptorSetLayout(device, renderer->sampler_set_layout, NULL);
            renderer->sampler_set_layout = VK_NULL_HANDLE;
        }

        vk_renderer_pipeline_destroy_all(&renderer->context, renderer->pipelines);

        if (renderer->pipeline_cache) {
            vkDestroyPipelineCache(device, renderer->pipeline_cache, NULL);
            renderer->pipeline_cache = VK_NULL_HANDLE;
        }

        if (renderer->render_pass) {
            vkDestroyRenderPass(device, renderer->render_pass, NULL);
            renderer->render_pass = VK_NULL_HANDLE;
        }
    }

    vk_renderer_commands_destroy(renderer, &renderer->command_pool);

    vk_renderer_context_destroy(&renderer->context);
}
