# Fresh output routing for the existing snapshot_sandbox_v1 owner.
PREPARATION_GOALS := $(if $(MAKECMDGOALS),$(MAKECMDGOALS),all)
PREPARATION_ALLOWED := all debug test-stable test-fisics-snapshot-inputs fisics-dependency-check run-headless-smoke release-contract release-package-self-test release-artifact-disposable
ifneq ($(filter-out $(PREPARATION_ALLOWED),$(PREPARATION_GOALS)),)
$(error Unsupported contained preparation goal)
endif
.DEFAULT_GOAL := preparation-dispatch
.PHONY: preparation-dispatch $(PREPARATION_GOALS)
$(PREPARATION_GOALS): preparation-dispatch
preparation-dispatch:
	@test "$(abspath $(TARGET_BUILD_ROOT))" = "$(abspath $(PREPARATION_ROOT))" || (echo "Preparation/build roots differ"; exit 1)
	@python3 tools/packaging/prepare_shared_copy.py --source third_party/codework_shared --output "$(PREPARATION_ROOT)/preparation-shared"
	@$(MAKE) PREPARATION_CHILD=1 PREPARATION_DISPATCH=0 SHARED_ROOT="$(abspath $(PREPARATION_ROOT))/preparation-shared" $(PREPARATION_GOALS)
