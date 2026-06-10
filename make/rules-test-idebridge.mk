.PHONY: test-idebridge-phase1
test-idebridge-phase1: $(IDEBRIDGE_OUT)
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling idebridge phase-1 runtime check..."
	@$(CC) $(CFLAGS) tests/idebridge_phase1_check.c src/core/Ipc/ide_ipc_server.c src/core/Ipc/ide_ipc_server_utils.c src/core/Ipc/ide_ipc_build_helpers.c src/core/Ipc/ide_ipc_query_helpers.c src/core/Ipc/ide_ipc_search_helpers.c src/core/Ipc/ide_ipc_path_guard.c src/core/Diagnostics/diagnostics_engine.c src/core/Diagnostics/diagnostic_explanations.c src/core/Diagnostics/diagnostic_context.c src/core/BuildSystem/build_diagnostics.c src/core/Analysis/analysis_symbols_store.c src/core/Analysis/analysis_token_store.c src/core/Analysis/analysis_units_store.c src/core/Analysis/analysis_build_graph_store.c src/core/Analysis/analysis_memory_report_store.c src/core/Analysis/library_index.c src/core/LoopKernel/mainthread_context.c src/app/GlobalInfo/workspace_prefs.c ../fisiCs/src/Compiler/diagnostic_metadata.c -o $(IDEBRIDGE_PHASE1_TEST_OUT) $(LIB_DIRS) -ljson-c -lSDL2 || (echo "idebridge phase-1 compile failed."; exit 1)
	@echo "Running idebridge phase-1 runtime check..."
	@$(IDEBRIDGE_PHASE1_TEST_OUT) || (echo "idebridge phase-1 runtime check failed."; exit 1)
	@echo "idebridge phase-1 runtime check passed."

.PHONY: test-idebridge-phase2
test-idebridge-phase2:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling idebridge phase-2 runtime check..."
	@$(CC) $(CFLAGS) tests/idebridge_phase2_check.c src/core/Ipc/ide_ipc_server.c src/core/Ipc/ide_ipc_server_utils.c src/core/Ipc/ide_ipc_build_helpers.c src/core/Ipc/ide_ipc_query_helpers.c src/core/Ipc/ide_ipc_search_helpers.c src/core/Ipc/ide_ipc_path_guard.c src/core/Diagnostics/diagnostics_engine.c src/core/Diagnostics/diagnostic_explanations.c src/core/Diagnostics/diagnostic_context.c src/core/BuildSystem/build_diagnostics.c src/core/Analysis/analysis_symbols_store.c src/core/Analysis/analysis_token_store.c src/core/Analysis/analysis_units_store.c src/core/Analysis/analysis_build_graph_store.c src/core/Analysis/analysis_memory_report_store.c src/core/Analysis/library_index.c src/core/LoopKernel/mainthread_context.c src/app/GlobalInfo/workspace_prefs.c ../fisiCs/src/Compiler/diagnostic_metadata.c -o $(IDEBRIDGE_PHASE2_TEST_OUT) $(LIB_DIRS) -ljson-c -lSDL2 || (echo "idebridge phase-2 compile failed."; exit 1)
	@echo "Running idebridge phase-2 runtime check..."
	@$(IDEBRIDGE_PHASE2_TEST_OUT) || (echo "idebridge phase-2 runtime check failed."; exit 1)
	@echo "idebridge phase-2 runtime check passed."

.PHONY: test-idebridge-phase3
test-idebridge-phase3:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling idebridge phase-3 runtime check..."
	@$(CC) $(CFLAGS) tests/idebridge_phase3_check.c src/core/Ipc/ide_ipc_server.c src/core/Ipc/ide_ipc_server_utils.c src/core/Ipc/ide_ipc_build_helpers.c src/core/Ipc/ide_ipc_query_helpers.c src/core/Ipc/ide_ipc_search_helpers.c src/core/Ipc/ide_ipc_path_guard.c src/core/Diagnostics/diagnostics_engine.c src/core/Diagnostics/diagnostic_explanations.c src/core/Diagnostics/diagnostic_context.c src/core/BuildSystem/build_diagnostics.c src/core/Analysis/analysis_symbols_store.c src/core/Analysis/analysis_token_store.c src/core/Analysis/analysis_units_store.c src/core/Analysis/analysis_build_graph_store.c src/core/Analysis/analysis_memory_report_store.c src/core/Analysis/library_index.c src/core/LoopKernel/mainthread_context.c src/app/GlobalInfo/workspace_prefs.c ../fisiCs/src/Compiler/diagnostic_metadata.c -o $(IDEBRIDGE_PHASE3_TEST_OUT) $(LIB_DIRS) -ljson-c -lSDL2 || (echo "idebridge phase-3 compile failed."; exit 1)
	@echo "Running idebridge phase-3 runtime check..."
	@$(IDEBRIDGE_PHASE3_TEST_OUT) || (echo "idebridge phase-3 runtime check failed."; exit 1)
	@echo "idebridge phase-3 runtime check passed."

.PHONY: test-idebridge-phase4
test-idebridge-phase4:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling idebridge phase-4 runtime check..."
	@$(CC) $(CFLAGS) tests/idebridge_phase4_check.c src/core/Ipc/ide_ipc_server.c src/core/Ipc/ide_ipc_server_utils.c src/core/Ipc/ide_ipc_build_helpers.c src/core/Ipc/ide_ipc_query_helpers.c src/core/Ipc/ide_ipc_search_helpers.c src/core/Ipc/ide_ipc_path_guard.c src/core/Diagnostics/diagnostics_engine.c src/core/Diagnostics/diagnostic_explanations.c src/core/Diagnostics/diagnostic_context.c src/core/BuildSystem/build_diagnostics.c src/core/Analysis/analysis_symbols_store.c src/core/Analysis/analysis_token_store.c src/core/Analysis/analysis_units_store.c src/core/Analysis/analysis_build_graph_store.c src/core/Analysis/analysis_memory_report_store.c src/core/Analysis/library_index.c src/core/LoopKernel/mainthread_context.c src/app/GlobalInfo/workspace_prefs.c ../fisiCs/src/Compiler/diagnostic_metadata.c -o $(IDEBRIDGE_PHASE4_TEST_OUT) $(LIB_DIRS) -ljson-c -lSDL2 || (echo "idebridge phase-4 compile failed."; exit 1)
	@echo "Running idebridge phase-4 runtime check..."
	@$(IDEBRIDGE_PHASE4_TEST_OUT) || (echo "idebridge phase-4 runtime check failed."; exit 1)
	@echo "idebridge phase-4 runtime check passed."

.PHONY: test-idebridge-phase5
test-idebridge-phase5: $(IDEBRIDGE_OUT)
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling idebridge phase-5 runtime check..."
	@$(CC) $(CFLAGS) tests/idebridge_phase5_check.c src/core/Ipc/ide_ipc_server.c src/core/Ipc/ide_ipc_server_utils.c src/core/Ipc/ide_ipc_build_helpers.c src/core/Ipc/ide_ipc_query_helpers.c src/core/Ipc/ide_ipc_search_helpers.c src/core/Ipc/ide_ipc_path_guard.c src/core/Diagnostics/diagnostics_engine.c src/core/Diagnostics/diagnostic_explanations.c src/core/Diagnostics/diagnostic_context.c src/core/BuildSystem/build_diagnostics.c src/core/Analysis/analysis_symbols_store.c src/core/Analysis/analysis_token_store.c src/core/Analysis/analysis_units_store.c src/core/Analysis/analysis_build_graph_store.c src/core/Analysis/analysis_memory_report_store.c src/core/Analysis/library_index.c src/core/LoopKernel/mainthread_context.c src/app/GlobalInfo/workspace_prefs.c ../fisiCs/src/Compiler/diagnostic_metadata.c -o $(IDEBRIDGE_PHASE5_TEST_OUT) $(LIB_DIRS) -ljson-c -lSDL2 || (echo "idebridge phase-5 compile failed."; exit 1)
	@echo "Running idebridge phase-5 runtime check..."
	@$(IDEBRIDGE_PHASE5_TEST_OUT) || (echo "idebridge phase-5 runtime check failed."; exit 1)
	@echo "idebridge phase-5 runtime check passed."

.PHONY: test-idebridge-phase6
test-idebridge-phase6: $(IDEBRIDGE_OUT)
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling idebridge phase-6 runtime check..."
	@$(CC) $(CFLAGS) tests/idebridge_phase6_check.c src/core/Ipc/ide_ipc_server.c src/core/Ipc/ide_ipc_server_utils.c src/core/Ipc/ide_ipc_build_helpers.c src/core/Ipc/ide_ipc_query_helpers.c src/core/Ipc/ide_ipc_search_helpers.c src/core/Ipc/ide_ipc_path_guard.c src/core/Diagnostics/diagnostics_engine.c src/core/Diagnostics/diagnostic_explanations.c src/core/Diagnostics/diagnostic_context.c src/core/BuildSystem/build_diagnostics.c src/core/Analysis/analysis_symbols_store.c src/core/Analysis/analysis_token_store.c src/core/Analysis/analysis_units_store.c src/core/Analysis/analysis_build_graph_store.c src/core/Analysis/analysis_memory_report_store.c src/core/Analysis/library_index.c src/core/LoopKernel/mainthread_context.c src/app/GlobalInfo/workspace_prefs.c ../fisiCs/src/Compiler/diagnostic_metadata.c -o $(IDEBRIDGE_PHASE6_TEST_OUT) $(LIB_DIRS) -ljson-c -lSDL2 || (echo "idebridge phase-6 compile failed."; exit 1)
	@echo "Running idebridge phase-6 runtime check..."
	@$(IDEBRIDGE_PHASE6_TEST_OUT) || (echo "idebridge phase-6 runtime check failed."; exit 1)
	@echo "idebridge phase-6 runtime check passed."

.PHONY: test-idebridge-regression
test-idebridge-regression: test-idebridge-all
	@echo "test-idebridge-regression completed."

.PHONY: test-idebridge-diag-pack-export
test-idebridge-diag-pack-export:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling idebridge diag-pack export check..."
	@$(CC) $(CFLAGS) tests/idebridge_diag_pack_export_check.c src/core/Diagnostics/diagnostics_pack_export.c $(CORE_PACK_DIR)/src/core_pack.c $(CORE_BASE_DIR)/src/core_base.c -o $(TEST_BUILD_DIR)/idebridge_diag_pack_export_check $(LIB_DIRS) -ljson-c || (echo "idebridge diag-pack export compile failed."; exit 1)
	@echo "Running idebridge diag-pack export check..."
	@$(TEST_BUILD_DIR)/idebridge_diag_pack_export_check || (echo "idebridge diag-pack export check failed."; exit 1)
	@echo "idebridge diag-pack export check passed."

.PHONY: test-idebridge-diag-core-data-export
test-idebridge-diag-core-data-export:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling idebridge diag core_data export check..."
	@$(CC) $(CFLAGS) tests/idebridge_diag_core_data_export_check.c src/core/Diagnostics/diagnostics_core_data_export.c $(CORE_DATA_DIR)/src/core_data.c $(CORE_IO_DIR)/src/core_io.c $(CORE_BASE_DIR)/src/core_base.c -o $(TEST_BUILD_DIR)/idebridge_diag_core_data_export_check $(LIB_DIRS) -ljson-c || (echo "idebridge diag core_data export compile failed."; exit 1)
	@echo "Running idebridge diag core_data export check..."
	@$(TEST_BUILD_DIR)/idebridge_diag_core_data_export_check || (echo "idebridge diag core_data export check failed."; exit 1)
	@echo "idebridge diag core_data export check passed."
