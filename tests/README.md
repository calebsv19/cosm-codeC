# Test Harness

The `tests/` tree currently hosts focused, ad-hoc checks that exercise parts
of the rendering stack and other subsystems outside the main IDE loop.

| File | Purpose |
| --- | --- |
| `vk_renderer_macro_check.c` | Smoke-test that the Vulkan renderer’s public headers compile in isolation and that key feature macros are defined as expected. Useful for validating toolchain setup when working on the optional Vulkan reference app (`src/engine/Render/vk_renderer_ref`). |
| `terminal_grid_phase1_check.c` | Runtime check for terminal emulator phase-1 behavior: chunked CSI parsing, SGR colors (16/256/truecolor), UTF-8 decoding across chunks, and OSC swallowing. |
| `terminal_text_api_check.c` | Compile-only check that terminal-specific text draw/measure APIs and terminal font accessors are available and type-compatible. |
| `terminal_codex_transcript_check.c` | Runtime check using a codex-like terminal transcript fixture (ANSI styles, UTF-8 symbols, OSC hyperlink metadata, wrapping) to validate final terminal polish behavior. |
| `idebridge_phase1_check.c` | Runtime check for phase-1 IDE IPC server and `idebridge ping` flow (valid ping, malformed request error, unknown command error, socket cleanup). |
| `idebridge_phase2_check.c` | Runtime check for phase-2 IPC commands: diagnostics aggregation/truncation, symbol filtering, and open success/failure contract. |
| `idebridge_phase3_check.c` | Runtime check for phase-3 IPC commands: includes graph payload, structured search results, and build success/failure result contracts. |
| `idebridge_phase4_check.c` | Runtime check for phase-4 patch apply contract: success apply payload shape, hash-mismatch rejection, `--no_hash_check` acceptance, and malformed diff failure path. |
| `idebridge_phase5_check.c` | Runtime check for phase-5 UX/scale behavior: `--socket` override routing, invalid-socket exit taxonomy, `--spill_file` JSON output, and `XDG_CACHE_HOME` socket-root compatibility. |
| `idebridge_phase6_check.c` | Integration regression check for request routing across critical commands (`ping`, `diag`, `symbols`, `includes`, `search`, `build`, `open`, `edit`) plus malformed JSON and unknown command failure contracts. |
| `idebridge_diag_pack_export_check.c` | Runtime check for `core_pack` diagnostics snapshot export contract (`IDHD` summary chunk + `IDJS` payload chunk) used by `idebridge diag-pack`. |
| `idebridge_diag_core_data_export_check.c` | Runtime check for `core_data` diagnostics snapshot contract (`ide_diagnostics_summary_v1`, `ide_diagnostics_rows_v1`) used by `idebridge diag-dataset`. |

As additional automated coverage is added, place new suites in this directory
and extend this document so future contributors can spot gaps quickly.
