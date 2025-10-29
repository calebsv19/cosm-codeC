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

- [`vk_renderer_ref/`](vk_renderer_ref/README.md) — Standalone Vulkan renderer reference used for experiments and validation.
