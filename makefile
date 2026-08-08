include make/config.mk
include make/target.mk
include make/shared.mk
include make/paths.mk
include make/shared-libs.mk
include make/flags.mk
include make/sources-app.mk
include make/objects.mk
include make/sources-tests.mk
# ===== RULES =====
include make/phony.mk
include make/rules-build.mk
include make/rules-runtime.mk
include make/rules-vulkan-runtime.mk
include make/package-macos.mk
include make/release.mk
include make/rules-test-groups.mk
include make/rules-test-idebridge.mk
include make/rules-test-runtime.mk
include make/rules-test-loop-events.mk
include make/rules-test-diagnostics.mk

-include $(DEP_FILES)
