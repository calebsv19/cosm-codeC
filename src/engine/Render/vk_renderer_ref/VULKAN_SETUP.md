# Vulkan Integration Notes

This document records how the SDL → Vulkan renderer bridge was brought online and the state of the system now that it is drawing correctly.

## Background

The IDE originally used SDL’s `SDL_Renderer` API. We replaced that renderer with a drop-in Vulkan implementation (`VkRenderer`) that mimics the SDL primitives through the compatibility header `vk_renderer_compat_sdl.h`. The high-level layout code still calls functions such as `SDL_RenderFillRect`, but those macros now translate into Vulkan command recording.

During the migration the swapchain presented successfully, but every captured frame only contained the clear colour. GPU captures and verbose logs showed that draw calls and vertex data were being recorded, yet nothing appeared on screen. The investigation tracked the issue through several layers of the pipeline until rasterisation finally produced visible pixels.

## Debugging Timeline

1. **Instrumented render context setup**  
   Added detailed logs inside `setRenderContext` and `vk_renderer_begin_frame` to confirm that the global render context held the Vulkan renderer pointer, that swapchain images were being targeted, and that the render pass and framebuffers matched the swapchain format/extents.

2. **Verified vertex data and buffer flushes**  
   Instrumentation in `vk_renderer_set_draw_color` and `flush_vertex_range` showed that the vertex buffer received sensible `x/y` positions and RGBA values (with `a == 1.0`), ruling out CPU-side colour or flush issues.

3. **Fragment shader sanity checks**  
   Temporarily forced `fill.frag` to output constant colours. A solid red test proved that the graphics pipeline and attachments could write to the swapchain. Switching to `vec4(fsColor.rgb, 1.0)` produced a black screen, which highlighted a separate problem: the vertex shader was not receiving its tint push constants.

4. **Push-constant stage mask fix**  
   Updated `push_basic_constants` so that the tint block is pushed to both vertex and fragment stages. This allowed `fill.vert` to multiply vertex colours by the expected tint values, restoring the intended colour output once `fill.frag` reverted to `outColor = fsColor`.

5. **Coordinate system correction**  
   The rendered UI appeared upside-down because each Vulkan vertex shader manually flipped the Y axis (`ndc.y = -ndc.y`). Vulkan’s clip space already maps `y = +1` to the top of the viewport when a positive-height viewport is used. Removing the manual flip from `fill.vert`, `line.vert`, and `textured.vert` fixed the orientation.

6. **Debug logging controls**  
   Two build-time flags were introduced in the makefile:
   - `VULKAN_RENDER_DEBUG` enables high-level diagnostic output.
   - `VULKAN_RENDER_DEBUG_FRAMES` enables per-frame logging (requires `VULKAN_RENDER_DEBUG=1` as well).

   All verbose logging in `vk_renderer.c` and `render_pipeline.c` now routes through `VK_RENDERER_DEBUG_LOG` / `VK_RENDERER_FRAME_DEBUG_LOG`, leaving release builds silent while still allowing deep traces when needed.

## Current State

- The IDE renders correctly via Vulkan, with all UI panes and textures appearing in the proper orientation.
- Push constants (`screenSize` + tint) are delivered to both shader stages, so colour modulation works as intended.
- Debug logging is completely optional: a normal `make` build emits no Vulkan/SDL render logs, while `make VULKAN_RENDER_DEBUG=1 VULKAN_RENDER_DEBUG_FRAMES=1` reproduces the detailed trace used during bring-up.
- The shader binaries (`*.spv`) match the updated GLSL sources (no Y flip, tint fix).

This summary should provide enough history for future maintainers or tooling to understand the quirks of the Vulkan layer and the knobs available for debugging. ***
