# Rendering Utilities

Common rendering helpers plus optional reference back ends live here.

| File | Responsibility |
| --- | --- |
| `render_pipeline.h/c` | High-level drawing entry point used by the frame loop. Iterates panes and calls their render callbacks. |
| `render_helpers.h/c` | SDL convenience wrappers (fill rects, draw borders, colour helpers). |
| `render_text_helpers.h/c` | Text rendering utilities (measuring and drawing strings via SDL_ttf). |
| `render_font.h/c` | Font asset loading/cache helpers. |
| `renderer_backend.h` | Abstracts differences between renderer implementations (SDL vs. Vulkan reference). |

## Subdirectories

- `shared/vk_renderer/` — Standalone Vulkan renderer library (headers in `include/`, sources in `src/`, shaders in `shaders/`) used as an optional SDL backend.

`render_ui_button.c` resolves the shared KitUI compact rounded appearance and
paints it through the IDE Vulkan renderer, with caption clipping and focus state.
The direct SDL adapter requires SDL_Renderer and must not receive VkRenderer.

Font replacement stages a complete new owned font set before releasing the
previous set. Failed reloads retain usable fonts; legacy selectors update any
aliased UI tier. Font commits reset clamp measurements, whose content key covers
the entire string. `make test-render-font-lifecycle` uses real SDL_ttf assets to
check rollback, ownership, long-buffer edits and repeated preset/zoom changes at
integer/fractional raster scales. This does not qualify pixel-exact destination
geometry, settings failure/persistence rollback or native IME workflows.

Authoring entry now pins one font set until Apply/Cancel. `beginFontPreview`,
`acceptFontPreview` and `cancelFontPreview` preserve partial tier aliases and
allow Cancel without reopening missing assets. An existing valid set rejects a
failed requested reload; legacy startup fallback remains. The lifecycle test
also covers settings rollback/no-save and actual authoring session hooks.

`render_text_pixels.c/h` expresses the existing app frame-coordinate text in
native drawable pixels through imported Vulkan float quads and intersected
scissors. Cached and transient text share it. Texture keys retain base-font and
revision/density identity; measured prefixes use the selected raster font and
enclosing logical widths. `make build-render-text-pixel-test` builds a standalone
production-path GPU/CPU oracle; execution requires an isolated native output
root and validation-layer setup, rather than an ordinary stable-test invocation.
