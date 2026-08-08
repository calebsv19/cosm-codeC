.PHONY: test-diagnostics-pipeline-integration
test-diagnostics-pipeline-integration:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling diagnostics pipeline integration test..."
	@$(CC) $(CFLAGS) tests/diagnostics_pipeline_integration_test.c src/core/Analysis/analysis_store.c $(ANALYSIS_ARTIFACT_IO_SRC) src/core/LoopResults/completed_results_queue.c src/core/LoopEvents/event_queue.c src/core/LoopEvents/event_invalidation_policy.c src/core/Diagnostics/diagnostics_engine.c src/core/LoopKernel/mainthread_context.c $(CORE_QUEUE_DIR)/src/core_queue.c -o $(TEST_BUILD_DIR)/diagnostics_pipeline_integration_test $(LIB_DIRS) -ljson-c -lSDL2 || (echo "diagnostics pipeline integration test compile failed."; exit 1)
	@echo "Running diagnostics pipeline integration test..."
	@$(TEST_BUILD_DIR)/diagnostics_pipeline_integration_test || (echo "diagnostics pipeline integration test failed."; exit 1)
	@echo "Diagnostics pipeline integration test passed."

.PHONY: test-analysis-store-diagnostics-metadata
test-analysis-store-diagnostics-metadata:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling analysis store diagnostics metadata test..."
	@$(CC) $(CFLAGS) tests/analysis_store_diagnostics_metadata_test.c src/core/Analysis/analysis_store.c $(ANALYSIS_ARTIFACT_IO_SRC) src/core/Diagnostics/diagnostics_engine.c src/core/LoopKernel/mainthread_context.c -o $(TEST_BUILD_DIR)/analysis_store_diagnostics_metadata_test $(LIB_DIRS) -ljson-c -lSDL2 || (echo "analysis store diagnostics metadata test compile failed."; exit 1)
	@echo "Running analysis store diagnostics metadata test..."
	@$(TEST_BUILD_DIR)/analysis_store_diagnostics_metadata_test || (echo "analysis store diagnostics metadata test failed."; exit 1)
	@echo "Analysis store diagnostics metadata test passed."

.PHONY: test-diagnostics-artifact-io
test-diagnostics-artifact-io:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling diagnostics artifact IO test..."
	@$(CC) $(CFLAGS) tests/diagnostics_artifact_io_test.c $(TEST_FIXTURE_UTILS_SRC) src/core/Diagnostics/diagnostics_engine.c -o $(TEST_BUILD_DIR)/diagnostics_artifact_io_test $(LIB_DIRS) -ljson-c || (echo "diagnostics artifact IO test compile failed."; exit 1)
	@echo "Running diagnostics artifact IO test..."
	@$(TEST_BUILD_DIR)/diagnostics_artifact_io_test || (echo "diagnostics artifact IO test failed."; exit 1)
	@echo "Diagnostics artifact IO test passed."

.PHONY: test-analysis-units-store
test-analysis-units-store:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling analysis units store test..."
	@$(CC) $(CFLAGS) tests/analysis_units_store_test.c $(TEST_FIXTURE_UTILS_SRC) src/core/Analysis/analysis_units_store.c $(ANALYSIS_ARTIFACT_IO_SRC) src/core/LoopKernel/mainthread_context.c -o $(TEST_BUILD_DIR)/analysis_units_store_test $(LIB_DIRS) -ljson-c -lSDL2 -lpthread || (echo "analysis units store test compile failed."; exit 1)
	@echo "Running analysis units store test..."
	@$(TEST_BUILD_DIR)/analysis_units_store_test || (echo "analysis units store test failed."; exit 1)
	@echo "Analysis units store test passed."

.PHONY: test-analysis-cache-manifest
test-analysis-cache-manifest:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling analysis cache manifest test..."
	@$(CC) $(CFLAGS) tests/analysis_cache_manifest_test.c $(TEST_FIXTURE_UTILS_SRC) src/core/Analysis/analysis_cache_manifest.c src/core/Analysis/analysis_startup_audit.c src/core/Analysis/analysis_snapshot.c src/core/Analysis/analysis_cache.c src/core/Analysis/include_graph.c $(ANALYSIS_ARTIFACT_IO_SRC) -o $(TEST_BUILD_DIR)/analysis_cache_manifest_test $(LIB_DIRS) -ljson-c -lpthread || (echo "analysis cache manifest test compile failed."; exit 1)
	@echo "Running analysis cache manifest test..."
	@$(TEST_BUILD_DIR)/analysis_cache_manifest_test || (echo "analysis cache manifest test failed."; exit 1)
	@echo "Analysis cache manifest test passed."

.PHONY: test-analysis-startup-audit
test-analysis-startup-audit:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling analysis startup audit test..."
	@$(CC) $(CFLAGS) tests/analysis_startup_audit_test.c $(TEST_FIXTURE_UTILS_SRC) src/core/Analysis/analysis_startup_audit.c src/core/Analysis/analysis_snapshot.c src/core/Analysis/analysis_cache.c src/core/Analysis/include_graph.c $(ANALYSIS_ARTIFACT_IO_SRC) -o $(TEST_BUILD_DIR)/analysis_startup_audit_test $(LIB_DIRS) -ljson-c -lpthread || (echo "analysis startup audit test compile failed."; exit 1)
	@echo "Running analysis startup audit test..."
	@$(TEST_BUILD_DIR)/analysis_startup_audit_test || (echo "analysis startup audit test failed."; exit 1)
	@echo "Analysis startup audit test passed."

.PHONY: test-include-path-resolver-security
test-include-path-resolver-security:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling include path resolver security test..."
	@$(CC) $(CFLAGS) tests/include_path_resolver_security_test.c src/core/Analysis/include_path_resolver.c $(ANALYSIS_ARTIFACT_IO_SRC) -o $(TEST_BUILD_DIR)/include_path_resolver_security_test $(LIB_DIRS) -ljson-c || (echo "include path resolver security test compile failed."; exit 1)
	@echo "Running include path resolver security test..."
	@$(TEST_BUILD_DIR)/include_path_resolver_security_test || (echo "include path resolver security test failed."; exit 1)
	@echo "Include path resolver security test passed."

.PHONY: test-analysis-incremental-policy
test-analysis-incremental-policy:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling analysis incremental policy test..."
	@$(CC) $(CFLAGS) tests/analysis_incremental_policy_test.c src/core/Analysis/analysis_incremental_policy.c -o $(TEST_BUILD_DIR)/analysis_incremental_policy_test $(LIB_DIRS) || (echo "analysis incremental policy test compile failed."; exit 1)
	@echo "Running analysis incremental policy test..."
	@$(TEST_BUILD_DIR)/analysis_incremental_policy_test || (echo "analysis incremental policy test failed."; exit 1)
	@echo "Analysis incremental policy test passed."

.PHONY: test-analysis-token-store-persistence
test-analysis-token-store-persistence:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling analysis token store persistence test..."
	@$(CC) $(CFLAGS) -DANALYSIS_TOKEN_STORE_PERSIST_LIMIT_BYTES=1024 tests/analysis_token_store_persistence_test.c $(TEST_FIXTURE_UTILS_SRC) src/core/Analysis/analysis_token_store.c $(ANALYSIS_ARTIFACT_IO_SRC) -o $(TEST_BUILD_DIR)/analysis_token_store_persistence_test $(LIB_DIRS) -ljson-c || (echo "analysis token store persistence test compile failed."; exit 1)
	@echo "Running analysis token store persistence test..."
	@$(TEST_BUILD_DIR)/analysis_token_store_persistence_test || (echo "analysis token store persistence test failed."; exit 1)
	@echo "Analysis token store persistence test passed."

.PHONY: test-analysis-build-graph-store
test-analysis-build-graph-store:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling analysis build graph store test..."
	@$(CC) $(CFLAGS) tests/analysis_build_graph_store_test.c $(TEST_FIXTURE_UTILS_SRC) src/core/Analysis/analysis_build_graph_store.c $(ANALYSIS_ARTIFACT_IO_SRC) -o $(TEST_BUILD_DIR)/analysis_build_graph_store_test $(LIB_DIRS) -ljson-c -lpthread || (echo "analysis build graph store test compile failed."; exit 1)
	@echo "Running analysis build graph store test..."
	@$(TEST_BUILD_DIR)/analysis_build_graph_store_test || (echo "analysis build graph store test failed."; exit 1)
	@echo "Analysis build graph store test passed."

.PHONY: test-analysis-memory-report-store
test-analysis-memory-report-store:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling analysis memory report store test..."
	@$(CC) $(CFLAGS) tests/analysis_memory_report_store_test.c $(TEST_FIXTURE_UTILS_SRC) src/core/Analysis/analysis_memory_report_store.c $(ANALYSIS_ARTIFACT_IO_SRC) -o $(TEST_BUILD_DIR)/analysis_memory_report_store_test $(LIB_DIRS) -ljson-c -lpthread || (echo "analysis memory report store test compile failed."; exit 1)
	@echo "Running analysis memory report store test..."
	@$(TEST_BUILD_DIR)/analysis_memory_report_store_test || (echo "analysis memory report store test failed."; exit 1)
	@echo "Analysis memory report store test passed."

.PHONY: test-analysis-refresh-view
test-analysis-refresh-view:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling analysis refresh view test..."
	@$(CC) $(CFLAGS) tests/analysis_refresh_view_test.c src/core/Analysis/analysis_refresh_view.c -o $(TEST_BUILD_DIR)/analysis_refresh_view_test $(LIB_DIRS) || (echo "analysis refresh view test compile failed."; exit 1)
	@echo "Running analysis refresh view test..."
	@$(TEST_BUILD_DIR)/analysis_refresh_view_test || (echo "analysis refresh view test failed."; exit 1)
	@echo "Analysis refresh view test passed."

.PHONY: test-diagnostic-explanations-cache
test-diagnostic-explanations-cache:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling diagnostic explanations cache test..."
	@$(CC) $(CFLAGS) tests/diagnostic_explanations_cache_test.c src/core/Diagnostics/diagnostic_explanations.c ../fisiCs/src/Compiler/diagnostic_metadata.c -o $(TEST_BUILD_DIR)/diagnostic_explanations_cache_test $(LIB_DIRS) -ljson-c || (echo "diagnostic explanations cache test compile failed."; exit 1)
	@echo "Running diagnostic explanations cache test..."
	@$(TEST_BUILD_DIR)/diagnostic_explanations_cache_test || (echo "diagnostic explanations cache test failed."; exit 1)
	@echo "Diagnostic explanations cache test passed."

.PHONY: test-diagnostic-context-cache
test-diagnostic-context-cache:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling diagnostic context cache test..."
	@$(CC) $(CFLAGS) tests/diagnostic_context_cache_test.c src/core/Diagnostics/diagnostic_context.c -o $(TEST_BUILD_DIR)/diagnostic_context_cache_test $(LIB_DIRS) -ljson-c || (echo "diagnostic context cache test compile failed."; exit 1)
	@echo "Running diagnostic context cache test..."
	@$(TEST_BUILD_DIR)/diagnostic_context_cache_test || (echo "diagnostic context cache test failed."; exit 1)
	@echo "Diagnostic context cache test passed."

.PHONY: test-errors-filter
test-errors-filter:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling Errors panel filter test..."
	@$(CC) $(CFLAGS) tests/errors_filter_test.c src/ide/Panes/ToolPanels/Errors/errors_filter.c src/core/Diagnostics/diagnostics_engine.c -o $(TEST_BUILD_DIR)/errors_filter_test $(LIB_DIRS) -ljson-c || (echo "Errors panel filter test compile failed."; exit 1)
	@echo "Running Errors panel filter test..."
	@$(TEST_BUILD_DIR)/errors_filter_test || (echo "Errors panel filter test failed."; exit 1)
	@echo "Errors panel filter test passed."

.PHONY: test-errors-units-detail
test-errors-units-detail:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling Errors panel units detail test..."
	@$(CC) $(CFLAGS) tests/errors_units_detail_test.c src/ide/Panes/ToolPanels/Errors/errors_units_detail.c src/core/Diagnostics/diagnostic_context.c src/core/Diagnostics/diagnostics_engine.c src/core/Analysis/analysis_units_store.c $(ANALYSIS_ARTIFACT_IO_SRC) src/core/LoopKernel/mainthread_context.c -o $(TEST_BUILD_DIR)/errors_units_detail_test $(LIB_DIRS) -ljson-c -lSDL2 -lpthread || (echo "Errors panel units detail test compile failed."; exit 1)
	@echo "Running Errors panel units detail test..."
	@$(TEST_BUILD_DIR)/errors_units_detail_test || (echo "Errors panel units detail test failed."; exit 1)
	@echo "Errors panel units detail test passed."

.PHONY: test-errors-context-detail
test-errors-context-detail:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling Errors panel context detail test..."
	@$(CC) $(CFLAGS) tests/errors_context_detail_test.c src/ide/Panes/ToolPanels/Errors/errors_context_detail.c src/core/Diagnostics/diagnostic_context.c src/core/Diagnostics/diagnostics_engine.c -o $(TEST_BUILD_DIR)/errors_context_detail_test $(LIB_DIRS) -ljson-c || (echo "Errors panel context detail test compile failed."; exit 1)
	@echo "Running Errors panel context detail test..."
	@$(TEST_BUILD_DIR)/errors_context_detail_test || (echo "Errors panel context detail test failed."; exit 1)
	@echo "Errors panel context detail test passed."

.PHONY: test-errors-diagnostic-detail
test-errors-diagnostic-detail:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling Errors panel diagnostic detail test..."
	@$(CC) $(CFLAGS) tests/errors_diagnostic_detail_test.c src/ide/Panes/ToolPanels/Errors/errors_diagnostic_detail.c src/ide/Panes/ToolPanels/Errors/errors_context_detail.c src/ide/Panes/ToolPanels/Errors/errors_units_detail.c src/core/Diagnostics/diagnostic_context.c src/core/Diagnostics/diagnostic_explanations.c src/core/Diagnostics/diagnostics_engine.c src/core/Analysis/analysis_units_store.c $(ANALYSIS_ARTIFACT_IO_SRC) src/core/LoopKernel/mainthread_context.c ../fisiCs/src/Compiler/diagnostic_metadata.c -o $(TEST_BUILD_DIR)/errors_diagnostic_detail_test $(LIB_DIRS) -ljson-c -lSDL2 -lpthread || (echo "Errors panel diagnostic detail test compile failed."; exit 1)
	@echo "Running Errors panel diagnostic detail test..."
	@$(TEST_BUILD_DIR)/errors_diagnostic_detail_test || (echo "Errors panel diagnostic detail test failed."; exit 1)
	@echo "Errors panel diagnostic detail test passed."

.PHONY: test-control-panel-units-tree
test-control-panel-units-tree:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling Control panel units tree test..."
	@$(CC) $(CFLAGS) tests/control_panel_units_tree_test.c src/ide/Panes/ControlPanel/control_panel_units_tree.c src/ide/Panes/ControlPanel/control_tree_payload.c src/ide/UI/Trees/ui_tree_node.c src/core/Analysis/analysis_units_store.c $(ANALYSIS_ARTIFACT_IO_SRC) src/core/Analysis/analysis_symbols_store.c src/core/LoopKernel/mainthread_context.c -o $(TEST_BUILD_DIR)/control_panel_units_tree_test $(LIB_DIRS) -ljson-c -lSDL2 || (echo "Control panel units tree test compile failed."; exit 1)
	@echo "Running Control panel units tree test..."
	@$(TEST_BUILD_DIR)/control_panel_units_tree_test || (echo "Control panel units tree test failed."; exit 1)
	@echo "Control panel units tree test passed."

.PHONY: test-control-tree-payload
test-control-tree-payload:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling Control tree payload test..."
	@$(CC) $(CFLAGS) tests/control_tree_payload_test.c src/ide/Panes/ControlPanel/control_tree_payload.c src/ide/UI/Trees/ui_tree_node.c -o $(TEST_BUILD_DIR)/control_tree_payload_test $(LIB_DIRS) -lSDL2 || (echo "Control tree payload test compile failed."; exit 1)
	@echo "Running Control tree payload test..."
	@$(TEST_BUILD_DIR)/control_tree_payload_test || (echo "Control tree payload test failed."; exit 1)
	@echo "Control tree payload test passed."

.PHONY: test-control-panel-active-file-provider
test-control-panel-active-file-provider:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling Control panel active file provider test..."
	@$(CC) $(CFLAGS) tests/control_panel_active_file_provider_test.c src/ide/Panes/ControlPanel/control_panel_active_file.c -o $(TEST_BUILD_DIR)/control_panel_active_file_provider_test $(LIB_DIRS) -lSDL2 -lSDL2_ttf || (echo "Control panel active file provider test compile failed."; exit 1)
	@echo "Running Control panel active file provider test..."
	@$(TEST_BUILD_DIR)/control_panel_active_file_provider_test || (echo "Control panel active file provider test failed."; exit 1)
	@echo "Control panel active file provider test passed."

.PHONY: test-control-panel-composite-tree
test-control-panel-composite-tree:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling Control panel composite tree test..."
	@$(CC) $(CFLAGS) tests/control_panel_composite_tree_test.c src/ide/Panes/ControlPanel/control_panel_composite_tree.c src/ide/Panes/ControlPanel/control_tree_payload.c src/ide/UI/Trees/ui_tree_node.c -o $(TEST_BUILD_DIR)/control_panel_composite_tree_test $(LIB_DIRS) -lSDL2 || (echo "Control panel composite tree test compile failed."; exit 1)
	@echo "Running Control panel composite tree test..."
	@$(TEST_BUILD_DIR)/control_panel_composite_tree_test || (echo "Control panel composite tree test failed."; exit 1)
	@echo "Control panel composite tree test passed."

.PHONY: test-control-panel-unit-focus-mode
test-control-panel-unit-focus-mode:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling Control panel unit focus mode test..."
	@$(CC) $(CFLAGS) tests/control_panel_unit_focus_mode_test.c src/ide/Panes/ControlPanel/control_panel_search_filters_helpers.c -o $(TEST_BUILD_DIR)/control_panel_unit_focus_mode_test $(LIB_DIRS) -lSDL2 || (echo "Control panel unit focus mode test compile failed."; exit 1)
	@echo "Running Control panel unit focus mode test..."
	@$(TEST_BUILD_DIR)/control_panel_unit_focus_mode_test || (echo "Control panel unit focus mode test failed."; exit 1)
	@echo "Control panel unit focus mode test passed."

.PHONY: test-editor-units-projection
test-editor-units-projection:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling editor units projection test..."
	@$(CC) $(CFLAGS) tests/editor_units_projection_test.c src/ide/Panes/Editor/editor_projection.c src/ide/Panes/ControlPanel/control_panel_units_tree.c src/ide/Panes/ControlPanel/control_tree_payload.c src/ide/UI/Trees/ui_tree_node.c src/core/Analysis/analysis_units_store.c $(ANALYSIS_ARTIFACT_IO_SRC) src/core/Analysis/analysis_symbols_store.c src/core/LoopKernel/mainthread_context.c -o $(TEST_BUILD_DIR)/editor_units_projection_test $(LIB_DIRS) -ljson-c -lSDL2 || (echo "editor units projection test compile failed."; exit 1)
	@echo "Running editor units projection test..."
	@$(TEST_BUILD_DIR)/editor_units_projection_test || (echo "editor units projection test failed."; exit 1)
	@echo "Editor units projection test passed."

.PHONY: test-editor-diagnostic-markers
test-editor-diagnostic-markers:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling editor diagnostic markers test..."
	@$(CC) $(CFLAGS) tests/editor_diagnostic_markers_test.c src/ide/Panes/Editor/Render/editor_diagnostic_markers.c src/core/Diagnostics/diagnostics_engine.c -o $(TEST_BUILD_DIR)/editor_diagnostic_markers_test $(LIB_DIRS) -ljson-c -lSDL2 -lSDL2_ttf || (echo "editor diagnostic markers test compile failed."; exit 1)
	@echo "Running editor diagnostic markers test..."
	@$(TEST_BUILD_DIR)/editor_diagnostic_markers_test || (echo "editor diagnostic markers test failed."; exit 1)
	@echo "Editor diagnostic markers test passed."

.PHONY: test-mainthread-context-scope-regression
test-mainthread-context-scope-regression:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling mainthread context scope regression test..."
	@$(CC) $(CFLAGS) tests/mainthread_context_scope_regression_test.c src/core/LoopKernel/mainthread_context.c -o $(TEST_BUILD_DIR)/mainthread_context_scope_regression_test $(LIB_DIRS) -lSDL2 || (echo "mainthread context scope regression test compile failed."; exit 1)
	@echo "Running mainthread context scope regression test..."
	@$(TEST_BUILD_DIR)/mainthread_context_scope_regression_test || (echo "mainthread context scope regression test failed."; exit 1)
	@echo "Mainthread context scope regression test passed."

.PHONY: test-loop-diag-config-regression
test-loop-diag-config-regression:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling loop diagnostics config regression test..."
	@$(CC) $(CFLAGS) tests/loop_diag_config_regression_test.c src/core/LoopDiagnostics/loop_diag_config.c -o $(TEST_BUILD_DIR)/loop_diag_config_regression_test $(LIB_DIRS) || (echo "loop diagnostics config regression test compile failed."; exit 1)
	@echo "Running loop diagnostics config regression test..."
	@$(TEST_BUILD_DIR)/loop_diag_config_regression_test || (echo "loop diagnostics config regression test failed."; exit 1)
	@echo "Loop diagnostics config regression test passed."
