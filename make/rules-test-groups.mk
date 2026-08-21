.PHONY: test-list
test-list:
	@echo "Smoke tests:      $(TEST_SMOKE_TARGETS)"
	@echo "Terminal tests:   $(TEST_TERMINAL_TARGETS)"
	@echo "IDE bridge stable: $(TEST_IDEBRIDGE_STABLE_TARGETS)"
	@echo "IDE bridge legacy: $(TEST_IDEBRIDGE_LEGACY_TARGETS)"
	@echo "Extended tests:   $(TEST_EXTENDED_TARGETS)"
	@echo "Scaffold stable:  test-fast + test-idebridge"
	@echo "Scaffold legacy:  test-idebridge-legacy + test-extended"
	@echo "Phase 1 gate:     test-fast + test-idebridge-all + test-extended"
	@echo "Phase 2 gate:     test-phase1 + test-fast"
	@echo "Phase 3 gate:     test-phase2 + test-fast"
	@echo "Phase 4 gate:     test-phase3 + test-fast"
	@echo "Phase 5 gate:     test-phase4 + test-fast"

.PHONY: test-fast
test-fast: $(TEST_SMOKE_TARGETS) test-workspace-authoring-projection
	@echo "test-fast completed."

.PHONY: test-idebridge
test-idebridge: $(TEST_IDEBRIDGE_STABLE_TARGETS)
	@echo "test-idebridge completed."

.PHONY: test-idebridge-legacy
test-idebridge-legacy: $(TEST_IDEBRIDGE_LEGACY_TARGETS)
	@echo "test-idebridge-legacy completed."

.PHONY: test-idebridge-all
test-idebridge-all: $(TEST_IDEBRIDGE_ALL_TARGETS)
	@echo "test-idebridge-all completed."

.PHONY: test-extended
test-extended: $(TEST_EXTENDED_TARGETS)
	@echo "test-extended completed."

.PHONY: test-stable
test-stable: test-fast test-idebridge
	@echo "test-stable completed."

.PHONY: test-legacy
test-legacy: test-idebridge-legacy test-extended
	@echo "test-legacy completed."

.PHONY: run-headless-smoke run-headless-smoke-internal
run-headless-smoke:
	@$(MAKE) BUILD_TOOLCHAIN="$(TEST_TOOLCHAIN)" run-headless-smoke-internal

run-headless-smoke-internal: all test-stable
	@echo "run-headless-smoke completed."

.PHONY: visual-harness
visual-harness: $(OUT)
	@echo "visual-harness build gate ready: $(OUT)"
	@echo "launch manual UI validation with: make -C ide run-ide-theme"

.PHONY: visual-artifact visual-proof
VISUAL_ARTIFACT_DIR ?= visual_artifacts
VISUAL_ARTIFACT_PATH ?= $(VISUAL_ARTIFACT_DIR)/ide_first_frame.bmp

visual-artifact: $(OUT)
	@mkdir -p "$(VISUAL_ARTIFACT_DIR)"
	@rm -f "$(VISUAL_ARTIFACT_PATH)"
	@IDE_VISUAL_ARTIFACT_ONCE=1 IDE_VISUAL_ARTIFACT_PATH="$(VISUAL_ARTIFACT_PATH)" ./$(OUT)
	@test -s "$(VISUAL_ARTIFACT_PATH)"
	@echo "visual-artifact ready: $(VISUAL_ARTIFACT_PATH)"

visual-proof: visual-artifact

.PHONY: test check test-internal
test:
	@$(MAKE) BUILD_TOOLCHAIN="$(TEST_TOOLCHAIN)" test-internal

test-internal: test-fast test-idebridge test-extended
	@echo "Full test suite completed."

check: test

.PHONY: test-phase1
test-phase1: test-fast test-idebridge-all test-extended
	@echo "Phase 1 gate completed."

.PHONY: test-phase2
test-phase2: test-phase1 test-fast
	@echo "Phase 2 gate completed."

.PHONY: test-phase3
test-phase3: test-phase2 test-fast
	@echo "Phase 3 gate completed."

.PHONY: test-phase4
test-phase4: test-phase3 test-fast
	@echo "Phase 4 gate completed."

.PHONY: test-phase5
test-phase5: test-phase4 test-fast
	@echo "Phase 5 gate completed."
