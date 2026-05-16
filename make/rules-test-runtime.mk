.PHONY: test-vk-macros
test-vk-macros:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling Vulkan SDL macro compatibility check..."
	@$(CC) $(CFLAGS) -c tests/vk_renderer_macro_check.c -o $(VK_MACRO_TEST_OBJ) || (echo "Vulkan macro compatibility check failed."; exit 1)
	@echo "Vulkan SDL macro compatibility check passed."

.PHONY: test-shared-theme-font-adapter
test-shared-theme-font-adapter:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling IDE shared theme/font adapter test..."
	@$(CC) -std=c99 -Wall -Wextra -MMD -MP $(INC_DIRS) \
		tests/shared_theme_font_adapter_test.c \
		src/ide/UI/shared_theme_font_adapter.c \
		src/app/GlobalInfo/runtime_paths.c \
		$(CORE_THEME_DIR)/src/core_theme.c \
		$(CORE_FONT_DIR)/src/core_font.c \
		$(CORE_BASE_DIR)/src/core_base.c \
		-o $(TEST_BUILD_DIR)/shared_theme_font_adapter_test \
		$(LIB_DIRS) -lSDL2 || (echo "shared theme/font adapter test compile failed."; exit 1)
	@echo "Running IDE shared theme/font adapter test..."
	@$(TEST_BUILD_DIR)/shared_theme_font_adapter_test || (echo "shared theme/font adapter test failed."; exit 1)
	@echo "IDE shared theme/font adapter test passed."

.PHONY: test-runtime-paths-resolution
test-runtime-paths-resolution:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling runtime paths resolution test..."
	@$(CC) $(CFLAGS) tests/runtime_paths_resolution_test.c src/app/GlobalInfo/runtime_paths.c -o $(RUNTIME_PATHS_TEST_OUT) $(LIB_DIRS) || (echo "runtime paths resolution test compile failed."; exit 1)
	@echo "Running runtime paths resolution test..."
	@$(RUNTIME_PATHS_TEST_OUT) || (echo "runtime paths resolution test failed."; exit 1)
	@echo "Runtime paths resolution test passed."

.PHONY: test-runtime-startup-defaults
test-runtime-startup-defaults:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling runtime startup defaults test..."
	@$(CC) $(CFLAGS) tests/runtime_startup_defaults_test.c src/app/GlobalInfo/runtime_startup_defaults.c -o $(RUNTIME_STARTUP_DEFAULTS_TEST_OUT) $(LIB_DIRS) || (echo "runtime startup defaults test compile failed."; exit 1)
	@echo "Running runtime startup defaults test..."
	@$(RUNTIME_STARTUP_DEFAULTS_TEST_OUT) || (echo "runtime startup defaults test failed."; exit 1)
	@echo "Runtime startup defaults test passed."

.PHONY: test-completed-results-queue
test-completed-results-queue:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling completed results queue test..."
	@$(CC) $(CFLAGS) tests/completed_results_queue_test.c src/core/LoopResults/completed_results_queue.c $(CORE_QUEUE_DIR)/src/core_queue.c -o $(TEST_BUILD_DIR)/completed_results_queue_test $(LIB_DIRS) -lSDL2 || (echo "completed results queue test compile failed."; exit 1)
	@echo "Running completed results queue test..."
	@$(TEST_BUILD_DIR)/completed_results_queue_test || (echo "completed results queue test failed."; exit 1)
	@echo "Completed results queue test passed."

.PHONY: test-analysis-scheduler-coalescing
test-analysis-scheduler-coalescing:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling analysis scheduler coalescing test..."
	@$(CC) $(CFLAGS) tests/analysis_scheduler_coalescing_test.c src/core/Analysis/analysis_scheduler.c -o $(TEST_BUILD_DIR)/analysis_scheduler_coalescing_test $(LIB_DIRS) -lSDL2 || (echo "analysis scheduler coalescing test compile failed."; exit 1)
	@echo "Running analysis scheduler coalescing test..."
	@$(TEST_BUILD_DIR)/analysis_scheduler_coalescing_test || (echo "analysis scheduler coalescing test failed."; exit 1)
	@echo "Analysis scheduler coalescing test passed."

.PHONY: test-editor-edit-transaction-debounce
test-editor-edit-transaction-debounce:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling editor edit transaction debounce test..."
	@$(CC) $(CFLAGS) tests/editor_edit_transaction_debounce_test.c src/ide/Panes/Editor/editor_edit_transaction_core.c -o $(TEST_BUILD_DIR)/editor_edit_transaction_debounce_test $(LIB_DIRS) -lSDL2 || (echo "editor edit transaction debounce test compile failed."; exit 1)
	@echo "Running editor edit transaction debounce test..."
	@$(TEST_BUILD_DIR)/editor_edit_transaction_debounce_test || (echo "editor edit transaction debounce test failed."; exit 1)
	@echo "Editor edit transaction debounce test passed."
