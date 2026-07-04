# Test Harness

The `tests/` tree currently hosts focused, ad-hoc checks that exercise parts
of the rendering stack and other subsystems outside the main IDE loop.

Shared test-local fixture helpers live in `test_fixture_utils.*`. Use those
for temporary workspace roots, `ide_files` setup, and small text/sparse-file
writes in analysis or diagnostics tests instead of adding new ad-hoc `/tmp`
setup helpers.

| File | Purpose |
| --- | --- |
| `test_fixture_utils.c` / `test_fixture_utils.h` | R5 test-local fixture helper for deterministic `/tmp` workspace roots, `ide_files` setup, small text writes, sparse file writes, and file-existence checks used by analysis and diagnostics tests. |
| `vk_renderer_macro_check.c` | Smoke-test that the Vulkan renderer’s public headers compile in isolation and that key feature macros are defined as expected. Useful for validating toolchain setup when working on the optional Vulkan reference app (`src/engine/Render/vk_renderer_ref`). |
| `terminal_grid_phase1_check.c` | Runtime check for terminal emulator phase-1 behavior: chunked CSI parsing, SGR colors (16/256/truecolor), UTF-8 decoding across chunks, and OSC swallowing. |
| `terminal_text_api_check.c` | Compile-only check that terminal-specific text draw/measure APIs and terminal font accessors are available and type-compatible. |
| `terminal_codex_transcript_check.c` | Runtime check using a codex-like terminal transcript fixture (ANSI styles, UTF-8 symbols, OSC hyperlink metadata, wrapping) to validate final terminal polish behavior. |
| `idebridge_phase1_check.c` | Runtime check for phase-1 IDE IPC server and `idebridge ping` flow (valid ping, malformed request error, unknown command error, socket cleanup). |
| `idebridge_phase2_check.c` | Runtime check for phase-2 IPC commands: diagnostics aggregation/truncation, symbol filtering, and open success/failure contract. |
| `idebridge_phase3_check.c` | Runtime check for phase-3 IPC commands: includes graph payload, structured search results, and build success/failure result contracts. |
| `idebridge_phase4_check.c` | Runtime check for phase-4 patch apply contract: success apply payload shape, hash-mismatch rejection, explicit single-file `--no_hash_check` acceptance with `hash_policy`, malformed diff policy rejection, and unchecked multi-file policy rejection. |
| `idebridge_phase5_check.c` | Runtime check for phase-5 UX/scale behavior: `--socket` override routing, invalid-socket exit taxonomy, `--spill_file` JSON output, and `XDG_CACHE_HOME` socket-root compatibility. |
| `idebridge_phase6_check.c` | Integration regression check for request routing across critical commands (`ping`, `diag`, `symbols`, `includes`, `search`, `build`, `open`, `edit`) plus malformed JSON and unknown command failure contracts. |
| `idebridge_diag_pack_export_check.c` | Runtime check for `core_pack` diagnostics snapshot export contract (`IDHD` summary chunk + `IDJS` payload chunk) used by `idebridge diag-pack`. |
| `idebridge_diag_core_data_export_check.c` | Runtime check for `core_data` diagnostics snapshot contract (`ide_diagnostics_summary_v1`, `ide_diagnostics_rows_v1`) used by `idebridge diag-dataset`. |
| `idebridge_error_format_test.c` | R3 check for stable non-JSON idebridge error lines with `stage`, `code`, `message`, and optional `detail` fields. |
| `completed_results_queue_test.c` | Runtime check for LoopResults queue semantics: global-sequence `pop_any` ordering across subsystem lanes and owned payload release behavior on pop/reset paths. |
| `analysis_scheduler_coalescing_test.c` | Runtime check for scheduler key coalescing semantics: latest-wins replacement per key, deterministic dequeue/start order across distinct keys, and index-lane reason normalization to `ANALYSIS_JOB_KEY_INDEX`. |
| `analysis_refresh_view_test.c` | Runtime check for the UI-facing analysis refresh status composition helper, including queued-save, running/progress, last-error reason, startup-audit reason, cached, and idle text inputs. |
| `startup_diagnostics_test.c` | R3 runtime startup diagnostics check for the resource-root, workspace-selection, and IPC availability snapshot plus bounded one-line summary formatting that omits auth tokens. |
| `build_trust_notice_test.c` | R4 check for stable terminal-facing trust notices before user-triggered build/run command execution, including action, working directory, command source, command display, and fallback wording. |
| `git_command_runner_test.c` | R4 check that Git tool-panel command execution uses argv/chdir process spawning instead of shell-mediated workspace paths, including a shell-sensitive directory fixture. |
| `workspace_context_test.c` | Runtime check for the read-only workspace context snapshot: project/workspace/build path capture and project-boundary path matching. |
| `editor_edit_transaction_debounce_test.c` | Synthetic timing test for Phase 2 edit-transaction debounce behavior: debounce timeout commit, cursor/focus/file-switch boundary commits, timer cancellation, and transaction counters. |
| `loop_events_queue_test.c` | Runtime check for Phase 3/4 event queue semantics: FIFO ordering, monotonic sequence IDs, bounded-capacity overflow accounting, deferred counter tracking, and multi-frame bounded-drain fairness (backlog decreases and processed count advances without starvation). |
| `loop_events_emission_contract_test.c` | Runtime check for Phase 3 emitter helpers: document and analysis event payload mapping, sequence ordering, and queue insertion via typed emitter APIs. |
| `loop_events_invalidation_policy_test.c` | Runtime check for Phase 3 event-driven invalidation routing policy: event type to target-pane set and invalidation/redraw intent mapping. |
| `loop_events_dispatch_integration_test.c` | Runtime integration check for Phase 3 dispatch path: queued events drained through dispatch visitor mutate pane dirty flags/reasons and frame redraw invalidation state with expected target scope. |
| `fisics_bridge_events_regression_test.c` | Phase 4.1 regression check that in-process `fisics_bridge` analysis updates emit `SymbolTreeUpdated` and `DiagnosticsUpdated` runtime events with stamps matching current symbol/diagnostics stores. |
| `analysis_store_stamp_regression_test.c` | Phase 4.2 regression check that diagnostics store stamp advances on deletion (`analysis_store_remove`) so stale-result guards can detect deletion-only updates. |
| `analysis_runtime_events_startup_regression_test.c` | Phase 4.2 regression check that startup store hydration emits deterministic runtime events (`DiagnosticsUpdated` then `SymbolTreeUpdated`) with payload stamps matching current diagnostics/symbol stores. |
| `analysis_store_published_stamp_regression_test.c` | Phase 4.2 regression check that diagnostics published-stamp watermark only advances via explicit publish calls (event dispatch path), not raw worker/store mutations. |
| `library_index_stamp_regression_test.c` | Phase 4.3 regression check that library-index combined stamp advances only after finalized mutations and that published-stamp watermark follows event-dispatch publication semantics. |
| `idle_efficiency_sanity_test.c` | Phase 4.4 idle-efficiency sanity lane: synthetic idle ticks with empty event/result queues must keep depth/counter snapshots stable (no background queue growth without input/work). |
| `diagnostics_pipeline_integration_test.c` | Phase 4.2 closure integration check covering diagnostics completed-result apply, stale-drop rejection, runtime event emission, event-dispatch invalidation targets, and published-stamp update semantics. |
| `mainthread_context_scope_regression_test.c` | Phase 5.4 regression check that debug non-owner scope helpers allow sanctioned non-owner guarded paths without violating owner-thread assertions. |
| `loop_diag_config_regression_test.c` | Phase 5.4 regression check for structured loop diagnostics env parsing contract (`IDE_LOOP_DIAG_FORMAT`, `IDE_LOOP_DIAG_JSON`, `IDE_EVENT_DIAG_LOG`, and wait override bounds). |
| `diagnostics_artifact_io_test.c` | R3 diagnostics artifact IO check for save/load status reports, first-run missing file handling, invalid JSON/root shape, malformed rows, and oversized artifacts. |
| `errors_filter_test.c` | Errors panel query matching check across message, file, hint, category, code, and stage fields. |
| `errors_units_detail_test.c` | Errors panel units-detail check for diagnostic context dimension rows and optional symbol unit attachments. |
| `errors_context_detail_test.c` | Errors panel context-detail check for include stack, macro trace, detail rows, and navigation targets from diagnostic context JSON. |
| `errors_diagnostic_detail_test.c` | R5 check for selected-diagnostic detail line model composition across severity, path/span, category/code/stage, message, hint, explanation, units, and context rows. |

As additional automated coverage is added, place new suites in this directory
and extend this document so future contributors can spot gaps quickly.

## Auth Contract Note

Mutating IPC commands (`open`, `build`, `edit`) in phase-2 through phase-6
tests include the session `auth_token` returned by the running IPC server.
This matches production IDE behavior and prevents false failures in integration
checks when auth enforcement tightens.
