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
| `completed_results_queue_test.c` | Runtime check for LoopResults queue semantics: global-sequence `pop_any` ordering across subsystem lanes and owned payload release behavior on pop/reset paths. |
| `analysis_scheduler_coalescing_test.c` | Runtime check for Phase 2 scheduler key coalescing semantics: latest-wins replacement per key, pending key tracking, and deterministic dequeue/start order across distinct keys. |
| `editor_edit_transaction_debounce_test.c` | Synthetic timing test for Phase 2 edit-transaction debounce behavior: debounce timeout commit, cursor/focus/file-switch boundary commits, timer cancellation, and transaction counters. |
| `loop_events_queue_test.c` | Runtime check for Phase 3 event queue core semantics: FIFO ordering, monotonic sequence IDs, bounded-capacity overflow accounting, and deferred counter tracking. |
| `loop_events_emission_contract_test.c` | Runtime check for Phase 3 emitter helpers: document and analysis event payload mapping, sequence ordering, and queue insertion via typed emitter APIs. |
| `loop_events_invalidation_policy_test.c` | Runtime check for Phase 3 event-driven invalidation routing policy: event type to target-pane set and invalidation/redraw intent mapping. |
| `loop_events_dispatch_integration_test.c` | Runtime integration check for Phase 3 dispatch path: queued events drained through dispatch visitor mutate pane dirty flags/reasons and frame redraw invalidation state with expected target scope. |
| `fisics_bridge_events_regression_test.c` | Phase 4.1 regression check that in-process `fisics_bridge` analysis updates emit `SymbolTreeUpdated` and `DiagnosticsUpdated` runtime events with stamps matching current symbol/diagnostics stores. |
| `analysis_store_stamp_regression_test.c` | Phase 4.2 regression check that diagnostics store stamp advances on deletion (`analysis_store_remove`) so stale-result guards can detect deletion-only updates. |
| `analysis_runtime_events_startup_regression_test.c` | Phase 4.2 regression check that startup store hydration emits deterministic runtime events (`DiagnosticsUpdated` then `SymbolTreeUpdated`) with payload stamps matching current diagnostics/symbol stores. |
| `analysis_store_published_stamp_regression_test.c` | Phase 4.2 regression check that diagnostics published-stamp watermark only advances via explicit publish calls (event dispatch path), not raw worker/store mutations. |
| `diagnostics_pipeline_integration_test.c` | Phase 4.2 closure integration check covering diagnostics completed-result apply, stale-drop rejection, runtime event emission, event-dispatch invalidation targets, and published-stamp update semantics. |

As additional automated coverage is added, place new suites in this directory
and extend this document so future contributors can spot gaps quickly.

## Auth Contract Note

Mutating IPC commands (`open`, `build`, `edit`) in phase-2 through phase-6
tests include the session `auth_token` returned by the running IPC server.
This matches production IDE behavior and prevents false failures in integration
checks when auth enforcement tightens.
