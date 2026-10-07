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
test-ide-ui-button-adapter: $(KIT_UI_LIB) $(KIT_RENDER_LIB) $(VK_RENDERER_LIB) $(VK_RUNTIME_LIB) $(CORE_THEME_LIB) $(CORE_FONT_LIB) $(CORE_BASE_LIB)
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
		$(VK_RUNTIME_LIB) \
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

.PHONY: test-project-directory-scan
TEST_SMOKE_TARGETS += test-project-directory-scan
test-project-directory-scan:
	@mkdir -p $(TEST_BUILD_DIR)
	@$(CC) $(CFLAGS) $(INC_DIRS) tests/project_directory_scan_test.c src/app/GlobalInfo/project.c src/app/GlobalInfo/workspace_startup_policy.c -o $(TEST_BUILD_DIR)/project_directory_scan_test $(LIB_DIRS)
	@$(TEST_BUILD_DIR)/project_directory_scan_test

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

.PHONY: test-workspace-authoring-projection
test-workspace-authoring-projection:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling workspace authoring projection test..."
	@$(CC) $(CFLAGS) $(INC_DIRS) tests/workspace_authoring_projection_test.c src/app/GlobalInfo/workspace_authoring_projection.c src/app/GlobalInfo/workspace_authoring_presentation_ui.c src/ide/Panes/IconBar/icon_bar.c -o $(WORKSPACE_AUTHORING_PROJECTION_TEST_OUT) $(LIB_DIRS) -lSDL2 || (echo "workspace authoring projection test compile failed."; exit 1)
	@echo "Running workspace authoring projection test..."
	@$(WORKSPACE_AUTHORING_PROJECTION_TEST_OUT) || (echo "workspace authoring projection test failed."; exit 1)
	@echo "Workspace authoring projection test passed."

.PHONY: test-workspace-authoring-profile
test-workspace-authoring-profile:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling workspace authoring profile test..."
	@$(CC) $(CFLAGS) $(INC_DIRS) tests/workspace_authoring_profile_test.c src/app/GlobalInfo/workspace_authoring_profile.c src/app/GlobalInfo/workspace_authoring_profile_path.c src/app/GlobalInfo/workspace_authoring_projection.c src/ide/Panes/IconBar/icon_bar.c $(CORE_PACK_DIR)/src/core_pack.c $(CORE_PANE_SNAPSHOT_DIR)/src/core_pane_snapshot.c $(CORE_PANE_MODULE_DIR)/src/core_pane_module.c $(CORE_BASE_DIR)/src/core_base.c -o $(WORKSPACE_AUTHORING_PROFILE_TEST_OUT) $(LIB_DIRS) -lSDL2 || (echo "workspace authoring profile test compile failed."; exit 1)
	@echo "Running workspace authoring profile test..."
	@$(WORKSPACE_AUTHORING_PROFILE_TEST_OUT) || (echo "workspace authoring profile test failed."; exit 1)
	@echo "Workspace authoring profile test passed."

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

.PHONY: test-editor-goto-framing
test-editor-goto-framing:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling editor goto framing test..."
	@$(CC) $(CFLAGS) tests/editor_goto_framing_test.c src/ide/Panes/Editor/editor_state.c -o $(EDITOR_GOTO_FRAMING_TEST_OUT) $(LIB_DIRS) -lSDL2 -lm || (echo "editor goto framing test compile failed."; exit 1)
	@echo "Running editor goto framing test..."
	@$(EDITOR_GOTO_FRAMING_TEST_OUT) || (echo "editor goto framing test failed."; exit 1)
	@echo "Editor goto framing test passed."

.PHONY: test-editor-cursor-preferences
test-editor-cursor-preferences:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling editor cursor preferences test..."
	@$(CC) $(CFLAGS) tests/editor_cursor_preferences_test.c src/ide/Panes/Editor/editor_cursor_preferences.c -o $(EDITOR_CURSOR_PREFERENCES_TEST_OUT) $(LIB_DIRS) -lSDL2 || (echo "editor cursor preferences test compile failed."; exit 1)
	@echo "Running editor cursor preferences test..."
	@$(EDITOR_CURSOR_PREFERENCES_TEST_OUT) || (echo "editor cursor preferences test failed."; exit 1)
	@echo "Editor cursor preferences test passed."

.PHONY: test-editor-session-control-panel
test-editor-session-control-panel:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling editor session Control panel test..."
	@$(CC) $(CFLAGS) tests/editor_session_control_panel_test.c src/ide/Panes/Editor/editor_session_control_panel.c -o $(EDITOR_SESSION_CONTROL_PANEL_TEST_OUT) $(LIB_DIRS) -ljson-c -lSDL2 || (echo "editor session Control panel test compile failed."; exit 1)
	@echo "Running editor session Control panel test..."
	@$(EDITOR_SESSION_CONTROL_PANEL_TEST_OUT) || (echo "editor session Control panel test failed."; exit 1)
	@echo "Editor session Control panel test passed."

.PHONY: test-ipc-document-control
test-ipc-document-control:
	@mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) tests/ide_ipc_document_test.c src/core/Ipc/ide_ipc_document.c -o $(TEST_BUILD_DIR)/ide_ipc_document_test $(LIB_DIRS) -ljson-c -lpthread
	$(TEST_BUILD_DIR)/ide_ipc_document_test
	$(CC) $(CFLAGS) tests/ide_ipc_document_ui_test.c src/core/Ipc/ide_ipc_document_ui.c src/core/Ipc/ide_ipc_path_guard.c -o $(TEST_BUILD_DIR)/ide_ipc_document_ui_test $(LIB_DIRS) -ljson-c -lSDL2
	$(TEST_BUILD_DIR)/ide_ipc_document_ui_test

.PHONY: test-ipc-workspace-control
test-ipc-workspace-control:
	@mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) tests/ide_ipc_workspace_test.c src/core/Ipc/ide_ipc_workspace.c -o $(TEST_BUILD_DIR)/ide_ipc_workspace_test $(LIB_DIRS) -ljson-c -lpthread
	$(TEST_BUILD_DIR)/ide_ipc_workspace_test

.PHONY: test-ide-menu-window-ui
test-ide-menu-window-ui: $(KIT_UI_LIB) $(KIT_RENDER_LIB) $(VK_RENDERER_LIB) $(VK_RUNTIME_LIB) $(CORE_THEME_LIB) $(CORE_FONT_LIB) $(CORE_BASE_LIB)
	@mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) -I$(TARGET_HOMEBREW_PREFIX)/include/SDL2 tests/ide_menu_bar_interaction_test.c src/ide/Panes/MenuBar/menu_bar_controls.c src/ide/Panes/MenuBar/input_menu_bar.c $(KIT_UI_DIR)/src/kit_ui_interaction_sdl.c -o $(TEST_BUILD_DIR)/ide_menu_bar_interaction_test $(KIT_UI_LIB) $(KIT_RENDER_LIB) $(VK_RENDERER_LIB) $(VK_RUNTIME_LIB) $(CORE_THEME_LIB) $(CORE_FONT_LIB) $(CORE_BASE_LIB) $(LIB_DIRS) -lSDL2 -lSDL2_ttf $(VULKAN_LIBS) -lm
	$(TEST_BUILD_DIR)/ide_menu_bar_interaction_test
	$(CC) $(CFLAGS) -I$(TARGET_HOMEBREW_PREFIX)/include/SDL2 tests/ide_window_lifecycle_test.c src/ide/UI/window_lifecycle.c src/core/InputManager/input_resize.c $(KIT_UI_DIR)/src/kit_ui_window_sdl.c -o $(TEST_BUILD_DIR)/ide_window_lifecycle_test $(CORE_BASE_LIB) $(LIB_DIRS) -lSDL2 $(VULKAN_LIBS) -lm
	$(TEST_BUILD_DIR)/ide_window_lifecycle_test

TEST_SMOKE_TARGETS += test-ide-menu-window-ui

.PHONY: test-ide-shared-panel-ui
test-ide-shared-panel-ui: $(KIT_UI_LIB) $(KIT_RENDER_LIB) $(VK_RENDERER_LIB) $(VK_RUNTIME_LIB) $(CORE_THEME_LIB) $(CORE_FONT_LIB) $(CORE_BASE_LIB)
	@mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) tests/ide_shared_panel_controls_test.c src/ide/Panes/IconBar/command_icon_bar.c src/ide/UI/button_scope.c src/ide/Panes/IconBar/icon_bar.c src/ide/Panes/IconBar/input_icon_bar.c src/ide/Panes/ToolPanels/Project/project_toolbar.c $(KIT_UI_DIR)/src/kit_ui_interaction_sdl.c -o $(TEST_BUILD_DIR)/ide_shared_panel_controls_test $(KIT_UI_LIB) $(KIT_RENDER_LIB) $(VK_RENDERER_LIB) $(VK_RUNTIME_LIB) $(CORE_THEME_LIB) $(CORE_FONT_LIB) $(CORE_BASE_LIB) $(LIB_DIRS) -lSDL2 -lSDL2_ttf $(VULKAN_LIBS) -lm
	$(TEST_BUILD_DIR)/ide_shared_panel_controls_test
	$(CC) $(CFLAGS) tests/ide_pane_composition_test.c src/ide/UI/pane_composition.c $(PANE_COMPOSITION_SRC) -o $(TEST_BUILD_DIR)/ide_pane_composition_test $(CORE_BASE_LIB) $(LIB_DIRS) -lSDL2 -lm
	$(TEST_BUILD_DIR)/ide_pane_composition_test
TEST_SMOKE_TARGETS += test-ide-shared-panel-ui

.PHONY: test-ide-panel-text-ui
test-ide-panel-text-ui: $(KIT_UI_LIB) $(KIT_RENDER_LIB) $(VK_RENDERER_LIB) $(VK_RUNTIME_LIB) $(CORE_THEME_LIB) $(CORE_FONT_LIB) $(CORE_BASE_LIB)
	@mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) tests/ide_panel_text_edit_test.c -o $(TEST_BUILD_DIR)/ide_panel_text_edit_test $(KIT_UI_LIB) $(CORE_BASE_LIB) $(LIB_DIRS) -lSDL2 -lm
	$(TEST_BUILD_DIR)/ide_panel_text_edit_test
	$(CC) $(CFLAGS) tests/ide_panel_text_presentation_test.c src/ide/UI/panel_text_presentation.c -o $(TEST_BUILD_DIR)/ide_panel_text_presentation_test $(KIT_UI_LIB) $(KIT_RENDER_LIB) $(CORE_BASE_LIB) $(LIB_DIRS) -lSDL2 -lm
	$(TEST_BUILD_DIR)/ide_panel_text_presentation_test
	$(CC) $(CFLAGS) tests/ide_panel_button_scope_test.c src/ide/UI/panel_button_scope.c src/ide/UI/button_scope.c $(KIT_UI_DIR)/src/kit_ui_interaction_sdl.c -o $(TEST_BUILD_DIR)/ide_panel_button_scope_test $(KIT_UI_LIB) $(KIT_RENDER_LIB) $(VK_RENDERER_LIB) $(VK_RUNTIME_LIB) $(CORE_THEME_LIB) $(CORE_FONT_LIB) $(CORE_BASE_LIB) $(LIB_DIRS) -lSDL2 -lSDL2_ttf $(VULKAN_LIBS) -lm
	$(TEST_BUILD_DIR)/ide_panel_button_scope_test
	$(CC) $(CFLAGS) tests/ide_panel_field_focus_test.c src/ide/UI/panel_field_focus.c -o $(TEST_BUILD_DIR)/ide_panel_field_focus_test
	$(TEST_BUILD_DIR)/ide_panel_field_focus_test
TEST_SMOKE_TARGETS += test-ide-panel-text-ui

.PHONY: test-ide-tool-chrome-ui
test-ide-tool-chrome-ui: $(KIT_UI_LIB) $(KIT_RENDER_LIB) $(VK_RENDERER_LIB) $(VK_RUNTIME_LIB) $(CORE_THEME_LIB) $(CORE_FONT_LIB) $(CORE_BASE_LIB)
	@mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) tests/ide_tool_chrome_test.c src/ide/UI/panel_button_scope.c src/ide/UI/button_scope.c src/ide/Panes/ToolPanels/Libraries/input_tool_libraries.c src/ide/Panes/ToolPanels/Tasks/input_tool_tasks.c src/ide/Panes/Terminal/terminal_header_controls.c src/ide/Panes/Terminal/terminal_header_actions.c $(KIT_UI_DIR)/src/kit_ui_interaction_sdl.c -o $(TEST_BUILD_DIR)/ide_tool_chrome_test $(KIT_UI_LIB) $(KIT_RENDER_LIB) $(VK_RENDERER_LIB) $(VK_RUNTIME_LIB) $(CORE_THEME_LIB) $(CORE_FONT_LIB) $(CORE_BASE_LIB) $(LIB_DIRS) -lSDL2 -lSDL2_ttf $(VULKAN_LIBS) -lm
	$(TEST_BUILD_DIR)/ide_tool_chrome_test
TEST_SMOKE_TARGETS += test-ide-tool-chrome-ui

.PHONY: test-ide-editor-header-ui
test-ide-editor-header-ui: $(KIT_UI_LIB) $(KIT_RENDER_LIB) $(VK_RENDERER_LIB) $(VK_RUNTIME_LIB) $(CORE_THEME_LIB) $(CORE_FONT_LIB) $(CORE_BASE_LIB)
	@mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) tests/ide_editor_header_test.c src/ide/Panes/Editor/editor_header_controls.c src/ide/Panes/Editor/Input/input_editor.c src/ide/UI/button_scope.c $(KIT_UI_DIR)/src/kit_ui_interaction_sdl.c -o $(TEST_BUILD_DIR)/ide_editor_header_test $(KIT_UI_LIB) $(KIT_RENDER_LIB) $(VK_RENDERER_LIB) $(VK_RUNTIME_LIB) $(CORE_THEME_LIB) $(CORE_FONT_LIB) $(CORE_BASE_LIB) $(LIB_DIRS) -lSDL2 -lSDL2_ttf $(VULKAN_LIBS) -lm
	$(TEST_BUILD_DIR)/ide_editor_header_test
TEST_SMOKE_TARGETS += test-ide-editor-header-ui

.PHONY: test-ide-rename-ui
test-ide-rename-ui: $(KIT_UI_LIB) $(KIT_RENDER_LIB) $(VK_RENDERER_LIB) $(VK_RUNTIME_LIB) $(CORE_THEME_LIB) $(CORE_FONT_LIB) $(CORE_BASE_LIB)
	@mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) tests/ide_rename_ui_test.c src/core/InputManager/UserInput/rename_flow.c -o $(TEST_BUILD_DIR)/ide_rename_ui_test $(KIT_UI_LIB) $(KIT_RENDER_LIB) $(VK_RENDERER_LIB) $(VK_RUNTIME_LIB) $(CORE_THEME_LIB) $(CORE_FONT_LIB) $(CORE_BASE_LIB) $(LIB_DIRS) -lSDL2 -lSDL2_ttf $(VULKAN_LIBS) -lm
	$(TEST_BUILD_DIR)/ide_rename_ui_test
TEST_SMOKE_TARGETS += test-ide-rename-ui

.PHONY: test-ide-diagnostics-cache
test-ide-diagnostics-cache:
	@mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(CFLAGS) tests/ide_build_diagnostics_cache_test.c src/core/BuildSystem/build_diagnostics.c src/core/Diagnostics/diagnostics_engine.c $(LIB_DIRS) -ljson-c -lpthread -o $(TEST_BUILD_DIR)/ide_build_diagnostics_cache_test
	$(TEST_BUILD_DIR)/ide_build_diagnostics_cache_test
