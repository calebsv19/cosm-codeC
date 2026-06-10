.PHONY: test-loop-events-queue
test-loop-events-queue:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling loop events queue test..."
	@$(CC) $(CFLAGS) tests/loop_events_queue_test.c src/core/LoopEvents/event_queue.c src/core/LoopKernel/mainthread_context.c -o $(TEST_BUILD_DIR)/loop_events_queue_test $(LIB_DIRS) -lSDL2 || (echo "loop events queue test compile failed."; exit 1)
	@echo "Running loop events queue test..."
	@$(TEST_BUILD_DIR)/loop_events_queue_test || (echo "loop events queue test failed."; exit 1)
	@echo "Loop events queue test passed."

.PHONY: test-loop-events-emission-contract
test-loop-events-emission-contract:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling loop events emission contract test..."
	@$(CC) $(CFLAGS) tests/loop_events_emission_contract_test.c src/core/LoopEvents/event_queue.c src/core/LoopKernel/mainthread_context.c -o $(TEST_BUILD_DIR)/loop_events_emission_contract_test $(LIB_DIRS) -lSDL2 || (echo "loop events emission contract test compile failed."; exit 1)
	@echo "Running loop events emission contract test..."
	@$(TEST_BUILD_DIR)/loop_events_emission_contract_test || (echo "loop events emission contract test failed."; exit 1)
	@echo "Loop events emission contract test passed."

.PHONY: test-loop-events-invalidation-policy
test-loop-events-invalidation-policy:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling loop events invalidation policy test..."
	@$(CC) $(CFLAGS) tests/loop_events_invalidation_policy_test.c tests/loop_events_invalidation_policy_test_stubs.c src/core/LoopEvents/event_invalidation_policy.c -o $(TEST_BUILD_DIR)/loop_events_invalidation_policy_test $(LIB_DIRS) -lSDL2 || (echo "loop events invalidation policy test compile failed."; exit 1)
	@echo "Running loop events invalidation policy test..."
	@$(TEST_BUILD_DIR)/loop_events_invalidation_policy_test || (echo "loop events invalidation policy test failed."; exit 1)
	@echo "Loop events invalidation policy test passed."

.PHONY: test-loop-events-dispatch-integration
test-loop-events-dispatch-integration:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling loop events dispatch integration test..."
	@$(CC) $(CFLAGS) tests/loop_events_dispatch_integration_test.c src/core/LoopEvents/event_queue.c src/core/LoopEvents/event_invalidation_policy.c src/core/LoopKernel/mainthread_context.c -o $(TEST_BUILD_DIR)/loop_events_dispatch_integration_test $(LIB_DIRS) -lSDL2 || (echo "loop events dispatch integration test compile failed."; exit 1)
	@echo "Running loop events dispatch integration test..."
	@$(TEST_BUILD_DIR)/loop_events_dispatch_integration_test || (echo "loop events dispatch integration test failed."; exit 1)
	@echo "Loop events dispatch integration test passed."

.PHONY: test-fisics-bridge-events-regression
test-fisics-bridge-events-regression:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling fisics bridge events regression test..."
	@$(CC) $(CFLAGS) tests/fisics_bridge_events_regression_test.c src/core/Analysis/fisics_bridge.c src/core/LoopEvents/event_queue.c src/core/Analysis/analysis_store.c src/core/Analysis/analysis_symbols_store.c src/core/Analysis/analysis_token_store.c src/core/Analysis/analysis_units_store.c src/core/Diagnostics/diagnostics_engine.c src/core/LoopKernel/mainthread_context.c $(CORE_QUEUE_DIR)/src/core_queue.c -o $(TEST_BUILD_DIR)/fisics_bridge_events_regression_test $(LIB_DIRS) -ljson-c -lSDL2 || (echo "fisics bridge events regression test compile failed."; exit 1)
	@echo "Running fisics bridge events regression test..."
	@$(TEST_BUILD_DIR)/fisics_bridge_events_regression_test || (echo "fisics bridge events regression test failed."; exit 1)
	@echo "Fisics bridge events regression test passed."

.PHONY: test-analysis-store-stamp-regression
test-analysis-store-stamp-regression:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling analysis store stamp regression test..."
	@$(CC) $(CFLAGS) tests/analysis_store_stamp_regression_test.c src/core/Analysis/analysis_store.c src/core/Diagnostics/diagnostics_engine.c src/core/LoopKernel/mainthread_context.c -o $(TEST_BUILD_DIR)/analysis_store_stamp_regression_test $(LIB_DIRS) -ljson-c -lSDL2 || (echo "analysis store stamp regression test compile failed."; exit 1)
	@echo "Running analysis store stamp regression test..."
	@$(TEST_BUILD_DIR)/analysis_store_stamp_regression_test || (echo "analysis store stamp regression test failed."; exit 1)
	@echo "Analysis store stamp regression test passed."

.PHONY: test-analysis-runtime-events-startup-regression
test-analysis-runtime-events-startup-regression:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling analysis runtime-events startup regression test..."
	@$(CC) $(CFLAGS) tests/analysis_runtime_events_startup_regression_test.c src/core/Analysis/analysis_runtime_events.c src/core/Analysis/analysis_store.c src/core/Analysis/analysis_symbols_store.c src/core/LoopEvents/event_queue.c src/core/Diagnostics/diagnostics_engine.c src/core/LoopKernel/mainthread_context.c $(CORE_QUEUE_DIR)/src/core_queue.c -o $(TEST_BUILD_DIR)/analysis_runtime_events_startup_regression_test $(LIB_DIRS) -ljson-c -lSDL2 || (echo "analysis runtime-events startup regression test compile failed."; exit 1)
	@echo "Running analysis runtime-events startup regression test..."
	@$(TEST_BUILD_DIR)/analysis_runtime_events_startup_regression_test || (echo "analysis runtime-events startup regression test failed."; exit 1)
	@echo "Analysis runtime-events startup regression test passed."

.PHONY: test-analysis-store-published-stamp-regression
test-analysis-store-published-stamp-regression:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling analysis store published-stamp regression test..."
	@$(CC) $(CFLAGS) tests/analysis_store_published_stamp_regression_test.c src/core/Analysis/analysis_store.c src/core/Diagnostics/diagnostics_engine.c src/core/LoopKernel/mainthread_context.c -o $(TEST_BUILD_DIR)/analysis_store_published_stamp_regression_test $(LIB_DIRS) -ljson-c -lSDL2 || (echo "analysis store published-stamp regression test compile failed."; exit 1)
	@echo "Running analysis store published-stamp regression test..."
	@$(TEST_BUILD_DIR)/analysis_store_published_stamp_regression_test || (echo "analysis store published-stamp regression test failed."; exit 1)
	@echo "Analysis store published-stamp regression test passed."

.PHONY: test-library-index-stamp-regression
test-library-index-stamp-regression:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling library index stamp regression test..."
	@$(CC) $(CFLAGS) tests/library_index_stamp_regression_test.c src/core/Analysis/library_index.c src/core/LoopKernel/mainthread_context.c -o $(TEST_BUILD_DIR)/library_index_stamp_regression_test $(LIB_DIRS) -ljson-c -lSDL2 -lpthread || (echo "library index stamp regression test compile failed."; exit 1)
	@echo "Running library index stamp regression test..."
	@$(TEST_BUILD_DIR)/library_index_stamp_regression_test || (echo "library index stamp regression test failed."; exit 1)
	@echo "Library index stamp regression test passed."

.PHONY: test-include-graph-snapshot
test-include-graph-snapshot:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling include graph snapshot test..."
	@$(CC) $(CFLAGS) tests/include_graph_snapshot_test.c src/core/Analysis/include_graph.c -o $(TEST_BUILD_DIR)/include_graph_snapshot_test $(LIB_DIRS) -ljson-c -lpthread || (echo "include graph snapshot test compile failed."; exit 1)
	@echo "Running include graph snapshot test..."
	@$(TEST_BUILD_DIR)/include_graph_snapshot_test || (echo "include graph snapshot test failed."; exit 1)
	@echo "Include graph snapshot test passed."

.PHONY: test-idle-efficiency-sanity
test-idle-efficiency-sanity:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling idle efficiency sanity test..."
	@$(CC) $(CFLAGS) tests/idle_efficiency_sanity_test.c src/core/LoopEvents/event_queue.c src/core/LoopResults/completed_results_queue.c src/core/LoopKernel/mainthread_context.c $(CORE_QUEUE_DIR)/src/core_queue.c -o $(TEST_BUILD_DIR)/idle_efficiency_sanity_test $(LIB_DIRS) -lSDL2 || (echo "idle efficiency sanity test compile failed."; exit 1)
	@echo "Running idle efficiency sanity test..."
	@$(TEST_BUILD_DIR)/idle_efficiency_sanity_test || (echo "idle efficiency sanity test failed."; exit 1)
	@echo "Idle efficiency sanity test passed."
