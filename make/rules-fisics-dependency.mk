# IDE owns dependency outputs; compiler source stays read-only.
FISICS_REQUIRED_SOURCE_HEAD := $(strip $(shell cat make/fisics-source-pin.txt))
FISICS_SOURCE_HEAD := $(strip $(shell python3 tools/packaging/verify_compiler_snapshot.py --root "$(FISICS_DIR)"))
FISICS_DEP_ROOT := $(abspath $(BUILD_DIR)/fisics-dependency/$(FISICS_SOURCE_HEAD))
FISICS_DEP_ARCHIVE := $(FISICS_DEP_ROOT)/libfisics_frontend.a
FISICS_DEP_BIN := $(FISICS_DEP_ROOT)/fisics
FISICS_FRONTEND_ARCHIVE_SRC = $(FISICS_LIB)

.PHONY: fisics-dependency-check fisics-package-compiler
fisics-dependency-check:
	@test "$(FISICS_SOURCE_HEAD)" = "$(FISICS_REQUIRED_SOURCE_HEAD)" || { echo "Compiler source revision drift"; exit 1; }
	@test -n "$(LLVM_CONFIG)" || { echo "Missing target llvm-config"; exit 1; }
	@python3 tools/packaging/verify_compiler_snapshot.py --root "$(FISICS_DIR)" >/dev/null

$(FISICS_LIB): FORCE fisics-dependency-check | $(SHARED_BUILD_DIR)
	@mkdir -p "$(FISICS_DEP_ROOT)"
	@$(MAKE) -C "$(FISICS_DIR)" BUILD_PROFILE="$(FISICS_FRONTEND_BUILD_PROFILE)" CC="$(HOST_CC) $(ARCH_FLAGS)" LLVM_CONFIG="$(LLVM_CONFIG)" BUILD_DIR="$(FISICS_DEP_ROOT)/obj" LIB_FRONTEND="$(FISICS_DEP_ARCHIVE)" frontend
	@cp "$(FISICS_DEP_ARCHIVE)" "$@"

fisics-package-compiler: $(FISICS_LIB)
	@$(MAKE) -C "$(FISICS_DIR)" BUILD_PROFILE="$(FISICS_FRONTEND_BUILD_PROFILE)" CC="$(HOST_CC) $(ARCH_FLAGS)" LLVM_CONFIG="$(LLVM_CONFIG)" BUILD_DIR="$(FISICS_DEP_ROOT)/obj" LIB_FRONTEND="$(FISICS_DEP_ARCHIVE)" BIN="$(FISICS_DEP_BIN)" "$(FISICS_DEP_BIN)"

.PHONY: test-fisics-snapshot-inputs
test-fisics-snapshot-inputs:
	@python3 -B tests/test_compiler_snapshot.py
	@python3 -B tests/test_release_disposable_roots.py
	@python3 -B tests/test_release_root_allocator.py
test-stable: test-fisics-snapshot-inputs
