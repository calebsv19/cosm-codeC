.PHONY: test-diagnostics-pipeline-integration
test-diagnostics-pipeline-integration:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling diagnostics pipeline integration test..."
	@$(CC) $(CFLAGS) tests/diagnostics_pipeline_integration_test.c src/core/Analysis/analysis_store.c src/core/LoopResults/completed_results_queue.c src/core/LoopEvents/event_queue.c src/core/LoopEvents/event_invalidation_policy.c src/core/Diagnostics/diagnostics_engine.c src/core/LoopKernel/mainthread_context.c $(CORE_QUEUE_DIR)/src/core_queue.c -o $(TEST_BUILD_DIR)/diagnostics_pipeline_integration_test $(LIB_DIRS) -ljson-c -lSDL2 || (echo "diagnostics pipeline integration test compile failed."; exit 1)
	@echo "Running diagnostics pipeline integration test..."
	@$(TEST_BUILD_DIR)/diagnostics_pipeline_integration_test || (echo "diagnostics pipeline integration test failed."; exit 1)
	@echo "Diagnostics pipeline integration test passed."

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
