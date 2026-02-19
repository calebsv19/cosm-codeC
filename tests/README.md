# Test Harness

The `tests/` tree currently hosts focused, ad-hoc checks that exercise parts
of the rendering stack and other subsystems outside the main IDE loop.

| File | Purpose |
| --- | --- |
| `vk_renderer_macro_check.c` | Smoke-test that the Vulkan renderer’s public headers compile in isolation and that key feature macros are defined as expected. Useful for validating toolchain setup when working on the optional Vulkan reference app (`src/engine/Render/vk_renderer_ref`). |
| `terminal_grid_phase1_check.c` | Runtime check for terminal emulator phase-1 behavior: chunked CSI parsing, SGR colors (16/256/truecolor), UTF-8 decoding across chunks, and OSC swallowing. |
| `terminal_text_api_check.c` | Compile-only check that terminal-specific text draw/measure APIs and terminal font accessors are available and type-compatible. |
| `terminal_codex_transcript_check.c` | Runtime check using a codex-like terminal transcript fixture (ANSI styles, UTF-8 symbols, OSC hyperlink metadata, wrapping) to validate final terminal polish behavior. |

As additional automated coverage is added, place new suites in this directory
and extend this document so future contributors can spot gaps quickly.
