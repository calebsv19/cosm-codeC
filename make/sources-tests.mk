IDEBRIDGE_SRCS := tools/idebridge/idebridge.c tools/idebridge/idebridge_file_utils.c tools/idebridge/idebridge_error_format.c
IDEBRIDGE_OBJS := $(patsubst tools/idebridge/%.c,$(TOOLS_BUILD_DIR)/idebridge_objs/%.o,$(IDEBRIDGE_SRCS))
IDEBRIDGE_DEP_FILES := $(IDEBRIDGE_OBJS:.o=.d)
ANALYSIS_ARTIFACT_IO_SRC := src/core/Analysis/analysis_artifact_io.c
TEST_FIXTURE_UTILS_SRC := tests/test_fixture_utils.c
IDEBRIDGE_PHASE_TEST_SUPPORT_SRCS := \
	src/core/Ipc/ide_ipc_server.c \
	src/core/Ipc/ide_ipc_server_utils.c \
	src/core/Ipc/ide_ipc_build_helpers.c \
	src/core/Ipc/ide_ipc_query_helpers.c \
	src/core/Ipc/ide_ipc_search_helpers.c \
	src/core/Ipc/ide_ipc_path_guard.c \
	src/core/Diagnostics/diagnostics_engine.c \
	src/core/Diagnostics/diagnostic_explanations.c \
	src/core/Diagnostics/diagnostic_context.c \
	src/core/BuildSystem/build_diagnostics.c \
	src/core/Analysis/analysis_symbols_store.c \
	src/core/Analysis/analysis_token_store.c \
	src/core/Analysis/analysis_units_store.c \
	$(ANALYSIS_ARTIFACT_IO_SRC) \
	src/core/Analysis/analysis_build_graph_store.c \
	src/core/Analysis/analysis_memory_report_store.c \
	src/core/Analysis/library_index.c \
	src/core/LoopKernel/mainthread_context.c \
	src/app/GlobalInfo/workspace_prefs.c \
	../fisiCs/src/Compiler/diagnostic_metadata.c
DIAG_PACK_EXPORT_OBJ := $(HOST_OBJ_DIR)/idebridge_support/core/Diagnostics/diagnostics_pack_export.o
DIAG_DATA_EXPORT_OBJ := $(HOST_OBJ_DIR)/idebridge_support/core/Diagnostics/diagnostics_core_data_export.o
IDEBRIDGE_LDFLAGS := $(LIB_DIRS) -ljson-c
VK_MACRO_TEST_OBJ := $(TEST_BUILD_DIR)/vk_renderer_macro_check.o
IDEBRIDGE_PHASE1_TEST_OUT := $(TEST_BUILD_DIR)/idebridge_phase1_check
IDEBRIDGE_PHASE2_TEST_OUT := $(TEST_BUILD_DIR)/idebridge_phase2_check
IDEBRIDGE_PHASE3_TEST_OUT := $(TEST_BUILD_DIR)/idebridge_phase3_check
IDEBRIDGE_PHASE4_TEST_OUT := $(TEST_BUILD_DIR)/idebridge_phase4_check
IDEBRIDGE_PHASE5_TEST_OUT := $(TEST_BUILD_DIR)/idebridge_phase5_check
IDEBRIDGE_PHASE6_TEST_OUT := $(TEST_BUILD_DIR)/idebridge_phase6_check
RUNTIME_PATHS_TEST_OUT := $(TEST_BUILD_DIR)/runtime_paths_resolution_test
RUNTIME_STARTUP_DEFAULTS_TEST_OUT := $(TEST_BUILD_DIR)/runtime_startup_defaults_test
STARTUP_DIAGNOSTICS_TEST_OUT := $(TEST_BUILD_DIR)/startup_diagnostics_test
WORKSPACE_STARTUP_POLICY_TEST_OUT := $(TEST_BUILD_DIR)/workspace_startup_policy_test
TERMINAL_GRID_PHASE1_TEST_OUT := $(TEST_BUILD_DIR)/terminal_grid_phase1_check
TERMINAL_CODEX_TRANSCRIPT_TEST_OUT := $(TEST_BUILD_DIR)/terminal_codex_transcript_check
TERMINAL_JOURNAL_TEST_OUT := $(TEST_BUILD_DIR)/terminal_journal_check
TERMINAL_TEXT_API_TEST_OBJ := $(TEST_BUILD_DIR)/terminal_text_api_check.o
TEST_IDEBRIDGE_STABLE_TARGETS := test-idebridge-error-format test-idebridge-phase1 test-idebridge-phase6
TEST_IDEBRIDGE_LEGACY_TARGETS := test-idebridge-phase2 test-idebridge-phase3 test-idebridge-phase4 test-idebridge-phase5
TEST_IDEBRIDGE_ALL_TARGETS := $(TEST_IDEBRIDGE_STABLE_TARGETS) $(TEST_IDEBRIDGE_LEGACY_TARGETS)
TEST_TERMINAL_TARGETS := test-terminal-grid-phase1 test-terminal-codex-transcript test-terminal-journal test-terminal-text-api
TEST_SMOKE_TARGETS := test-vk-macros test-shared-theme-font-adapter test-runtime-paths-resolution test-runtime-startup-defaults test-startup-diagnostics test-build-trust-notice test-git-command-runner test-workspace-startup-policy test-workspace-context $(TEST_TERMINAL_TARGETS) test-completed-results-queue test-analysis-scheduler-coalescing test-editor-edit-transaction-debounce test-loop-events-queue test-loop-events-emission-contract test-loop-events-invalidation-policy test-loop-events-dispatch-integration test-fisics-bridge-events-regression test-analysis-store-stamp-regression test-analysis-runtime-events-startup-regression test-analysis-store-published-stamp-regression test-library-index-stamp-regression test-include-graph-snapshot test-idle-efficiency-sanity test-diagnostics-pipeline-integration test-analysis-store-diagnostics-metadata test-diagnostics-artifact-io test-analysis-units-store test-analysis-cache-manifest test-analysis-startup-audit test-include-path-resolver-security test-analysis-incremental-policy test-analysis-token-store-persistence test-analysis-build-graph-store test-analysis-memory-report-store test-analysis-refresh-view test-diagnostic-explanations-cache test-diagnostic-context-cache test-errors-filter test-errors-units-detail test-errors-context-detail test-errors-diagnostic-detail test-control-panel-units-tree test-control-panel-unit-focus-mode test-editor-units-projection test-editor-diagnostic-markers test-mainthread-context-scope-regression test-loop-diag-config-regression
TEST_EXTENDED_TARGETS := test-idebridge-diag-pack-export test-idebridge-diag-core-data-export
DEP_FILES := $(APP_DEP_FILES) $(TIMER_HUD_DEP_FILES) $(IDEBRIDGE_SUPPORT_DEP_FILES) $(IDEBRIDGE_DEP_FILES)
