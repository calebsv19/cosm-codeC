# Test Harness

The `tests/` tree currently hosts focused, ad-hoc checks that exercise parts
of the rendering stack and other subsystems outside the main IDE loop.

| File | Purpose |
| --- | --- |
| `vk_renderer_macro_check.c` | Smoke-test that the Vulkan renderer’s public headers compile in isolation and that key feature macros are defined as expected. Useful for validating toolchain setup when working on the optional Vulkan reference app (`src/engine/Render/vk_renderer_ref`). |

As additional automated coverage is added, place new suites in this directory
and extend this document so future contributors can spot gaps quickly.
