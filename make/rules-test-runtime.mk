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

.PHONY: test-ide-ui-button-adapter
test-ide-ui-button-adapter: $(KIT_UI_LIB) $(KIT_RENDER_LIB) $(VK_RENDERER_LIB) $(CORE_THEME_LIB) $(CORE_FONT_LIB) $(CORE_BASE_LIB)
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling IDE UI button adapter test..."
	@$(CC) -std=c99 -Wall -Wextra -MMD -MP $(INC_DIRS) \
		tests/ide_ui_button_adapter_test.c \
		src/ide/UI/ide_ui_button.c \
		src/ide/UI/shared_theme_font_adapter.c \
		src/app/GlobalInfo/runtime_paths.c \
		$(KIT_UI_LIB) \
		$(KIT_RENDER_LIB) \
		$(VK_RENDERER_LIB) \
		$(CORE_THEME_LIB) \
		$(CORE_FONT_LIB) \
		$(CORE_BASE_LIB) \
		-o $(IDE_UI_BUTTON_ADAPTER_TEST_OUT) \
		$(LIB_DIRS) -lSDL2 -lSDL2_ttf -lvulkan || (echo "IDE UI button adapter test compile failed."; exit 1)
	@echo "Running IDE UI button adapter test..."
	@$(IDE_UI_BUTTON_ADAPTER_TEST_OUT) || (echo "IDE UI button adapter test failed."; exit 1)
	@echo "IDE UI button adapter test passed."

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

.PHONY: test-startup-diagnostics
test-startup-diagnostics:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling startup diagnostics test..."
	@$(CC) $(CFLAGS) tests/startup_diagnostics_test.c src/app/GlobalInfo/startup_diagnostics.c -o $(STARTUP_DIAGNOSTICS_TEST_OUT) $(LIB_DIRS) || (echo "startup diagnostics test compile failed."; exit 1)
	@echo "Running startup diagnostics test..."
	@$(STARTUP_DIAGNOSTICS_TEST_OUT) || (echo "startup diagnostics test failed."; exit 1)
	@echo "Startup diagnostics test passed."

.PHONY: test-build-trust-notice
test-build-trust-notice:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling build trust notice test..."
	@$(CC) $(CFLAGS) tests/build_trust_notice_test.c src/core/BuildSystem/build_trust_notice.c -o $(TEST_BUILD_DIR)/build_trust_notice_test $(LIB_DIRS) || (echo "build trust notice test compile failed."; exit 1)
	@echo "Running build trust notice test..."
	@$(TEST_BUILD_DIR)/build_trust_notice_test || (echo "build trust notice test failed."; exit 1)
	@echo "Build trust notice test passed."

.PHONY: test-git-command-runner
test-git-command-runner:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling Git command runner test..."
	@$(CC) $(CFLAGS) tests/git_command_runner_test.c src/ide/Panes/ToolPanels/Git/git_command_runner.c -o $(TEST_BUILD_DIR)/git_command_runner_test $(LIB_DIRS) || (echo "Git command runner test compile failed."; exit 1)
	@echo "Running Git command runner test..."
	@$(TEST_BUILD_DIR)/git_command_runner_test || (echo "Git command runner test failed."; exit 1)
	@echo "Git command runner test passed."

.PHONY: test-workspace-startup-policy
test-workspace-startup-policy:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling workspace startup policy test..."
	@$(CC) $(CFLAGS) $(INC_DIRS) tests/workspace_startup_policy_test.c src/app/GlobalInfo/workspace_startup_policy.c -o $(WORKSPACE_STARTUP_POLICY_TEST_OUT) $(LIB_DIRS) || (echo "workspace startup policy test compile failed."; exit 1)
	@echo "Running workspace startup policy test..."
	@$(WORKSPACE_STARTUP_POLICY_TEST_OUT) || (echo "workspace startup policy test failed."; exit 1)
	@echo "Workspace startup policy test passed."

.PHONY: test-workspace-context
test-workspace-context:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling workspace context test..."
	@$(CC) $(CFLAGS) tests/workspace_context_test.c src/app/GlobalInfo/workspace_context.c -o $(TEST_BUILD_DIR)/workspace_context_test $(LIB_DIRS) || (echo "workspace context test compile failed."; exit 1)
	@echo "Running workspace context test..."
	@$(TEST_BUILD_DIR)/workspace_context_test || (echo "workspace context test failed."; exit 1)
	@echo "Workspace context test passed."

.PHONY: test-terminal-grid-phase1
test-terminal-grid-phase1:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling terminal grid phase-1 test..."
	@$(CC) $(CFLAGS) tests/terminal_grid_phase1_check.c src/ide/Panes/Terminal/terminal_grid.c src/ide/Panes/Terminal/terminal_grid_buffer.c src/ide/Panes/Terminal/terminal_grid_sgr_helpers.c -o $(TERMINAL_GRID_PHASE1_TEST_OUT) $(LIB_DIRS) || (echo "terminal grid phase-1 compile failed."; exit 1)
	@echo "Running terminal grid phase-1 test..."
	@$(TERMINAL_GRID_PHASE1_TEST_OUT) || (echo "terminal grid phase-1 test failed."; exit 1)
	@echo "Terminal grid phase-1 test passed."

.PHONY: test-terminal-codex-transcript
test-terminal-codex-transcript:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling terminal Codex transcript test..."
	@$(CC) $(CFLAGS) tests/terminal_codex_transcript_check.c src/ide/Panes/Terminal/terminal_grid.c src/ide/Panes/Terminal/terminal_grid_buffer.c src/ide/Panes/Terminal/terminal_grid_sgr_helpers.c -o $(TERMINAL_CODEX_TRANSCRIPT_TEST_OUT) $(LIB_DIRS) || (echo "terminal Codex transcript compile failed."; exit 1)
	@echo "Running terminal Codex transcript test..."
	@$(TERMINAL_CODEX_TRANSCRIPT_TEST_OUT) || (echo "terminal Codex transcript test failed."; exit 1)
	@echo "Terminal Codex transcript test passed."

.PHONY: test-terminal-journal
test-terminal-journal:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling terminal journal test..."
	@$(CC) $(CFLAGS) tests/terminal_journal_check.c src/ide/Panes/Terminal/terminal_journal.c src/ide/Panes/Terminal/terminal_grid.c src/ide/Panes/Terminal/terminal_grid_buffer.c src/ide/Panes/Terminal/terminal_grid_sgr_helpers.c -o $(TERMINAL_JOURNAL_TEST_OUT) $(LIB_DIRS) || (echo "terminal journal compile failed."; exit 1)
	@echo "Running terminal journal test..."
	@$(TERMINAL_JOURNAL_TEST_OUT) || (echo "terminal journal test failed."; exit 1)
	@echo "Terminal journal test passed."

.PHONY: test-terminal-text-api
test-terminal-text-api:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling terminal text API check..."
	@$(CC) $(CFLAGS) -c tests/terminal_text_api_check.c -o $(TERMINAL_TEXT_API_TEST_OBJ) || (echo "terminal text API compile failed."; exit 1)
	@echo "Terminal text API check passed."

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
