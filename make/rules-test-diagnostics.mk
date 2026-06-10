.PHONY: test-diagnostics-pipeline-integration
test-diagnostics-pipeline-integration:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling diagnostics pipeline integration test..."
	@$(CC) $(CFLAGS) tests/diagnostics_pipeline_integration_test.c src/core/Analysis/analysis_store.c src/core/LoopResults/completed_results_queue.c src/core/LoopEvents/event_queue.c src/core/LoopEvents/event_invalidation_policy.c src/core/Diagnostics/diagnostics_engine.c src/core/LoopKernel/mainthread_context.c $(CORE_QUEUE_DIR)/src/core_queue.c -o $(TEST_BUILD_DIR)/diagnostics_pipeline_integration_test $(LIB_DIRS) -ljson-c -lSDL2 || (echo "diagnostics pipeline integration test compile failed."; exit 1)
	@echo "Running diagnostics pipeline integration test..."
	@$(TEST_BUILD_DIR)/diagnostics_pipeline_integration_test || (echo "diagnostics pipeline integration test failed."; exit 1)
	@echo "Diagnostics pipeline integration test passed."

.PHONY: test-analysis-store-diagnostics-metadata
test-analysis-store-diagnostics-metadata:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling analysis store diagnostics metadata test..."
	@$(CC) $(CFLAGS) tests/analysis_store_diagnostics_metadata_test.c src/core/Analysis/analysis_store.c src/core/Diagnostics/diagnostics_engine.c src/core/LoopKernel/mainthread_context.c -o $(TEST_BUILD_DIR)/analysis_store_diagnostics_metadata_test $(LIB_DIRS) -ljson-c -lSDL2 || (echo "analysis store diagnostics metadata test compile failed."; exit 1)
	@echo "Running analysis store diagnostics metadata test..."
	@$(TEST_BUILD_DIR)/analysis_store_diagnostics_metadata_test || (echo "analysis store diagnostics metadata test failed."; exit 1)
	@echo "Analysis store diagnostics metadata test passed."

.PHONY: test-analysis-units-store
test-analysis-units-store:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling analysis units store test..."
	@$(CC) $(CFLAGS) tests/analysis_units_store_test.c src/core/Analysis/analysis_units_store.c src/core/LoopKernel/mainthread_context.c -o $(TEST_BUILD_DIR)/analysis_units_store_test $(LIB_DIRS) -ljson-c -lSDL2 -lpthread || (echo "analysis units store test compile failed."; exit 1)
	@echo "Running analysis units store test..."
	@$(TEST_BUILD_DIR)/analysis_units_store_test || (echo "analysis units store test failed."; exit 1)
	@echo "Analysis units store test passed."

.PHONY: test-analysis-build-graph-store
test-analysis-build-graph-store:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling analysis build graph store test..."
	@$(CC) $(CFLAGS) tests/analysis_build_graph_store_test.c src/core/Analysis/analysis_build_graph_store.c -o $(TEST_BUILD_DIR)/analysis_build_graph_store_test $(LIB_DIRS) -ljson-c -lpthread || (echo "analysis build graph store test compile failed."; exit 1)
	@echo "Running analysis build graph store test..."
	@$(TEST_BUILD_DIR)/analysis_build_graph_store_test || (echo "analysis build graph store test failed."; exit 1)
	@echo "Analysis build graph store test passed."

.PHONY: test-analysis-memory-report-store
test-analysis-memory-report-store:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling analysis memory report store test..."
	@$(CC) $(CFLAGS) tests/analysis_memory_report_store_test.c src/core/Analysis/analysis_memory_report_store.c -o $(TEST_BUILD_DIR)/analysis_memory_report_store_test $(LIB_DIRS) -ljson-c -lpthread || (echo "analysis memory report store test compile failed."; exit 1)
	@echo "Running analysis memory report store test..."
	@$(TEST_BUILD_DIR)/analysis_memory_report_store_test || (echo "analysis memory report store test failed."; exit 1)
	@echo "Analysis memory report store test passed."

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
