# IDE owns dependency outputs; compiler source stays read-only.
FISICS_REQUIRED_SOURCE_HEAD := 83af7ab9c984981ae1f8044dd0f5b12caecfbaa9
FISICS_SOURCE_HEAD := $(shell git -C "$(FISICS_DIR)" rev-parse HEAD)
FISICS_DEP_ROOT := $(abspath $(BUILD_DIR)/fisics-dependency/$(FISICS_SOURCE_HEAD))
FISICS_DEP_ARCHIVE := $(FISICS_DEP_ROOT)/libfisics_frontend.a
FISICS_DEP_BIN := $(FISICS_DEP_ROOT)/fisics
FISICS_FRONTEND_ARCHIVE_SRC = $(FISICS_LIB)

.PHONY: fisics-dependency-check fisics-package-compiler
fisics-dependency-check:
	@test "$(FISICS_SOURCE_HEAD)" = "$(FISICS_REQUIRED_SOURCE_HEAD)" || { echo "Compiler source revision drift"; exit 1; }
	@test -n "$(LLVM_CONFIG)" || { echo "Missing target llvm-config"; exit 1; }
	@git -C "$(FISICS_DIR)" diff --quiet HEAD -- Makefile VERSION src include third_party || { echo "Compiler build inputs differ from committed source"; exit 1; }
	@test -z "$$(git -C "$(FISICS_DIR)" ls-files --others --exclude-standard -- src include third_party)" || { echo "Untracked compiler build inputs"; exit 1; }

$(FISICS_LIB): FORCE fisics-dependency-check | $(SHARED_BUILD_DIR)
	@mkdir -p "$(FISICS_DEP_ROOT)"
	@$(MAKE) -C "$(FISICS_DIR)" BUILD_PROFILE="$(FISICS_FRONTEND_BUILD_PROFILE)" CC="$(HOST_CC) $(ARCH_FLAGS)" LLVM_CONFIG="$(LLVM_CONFIG)" BUILD_DIR="$(FISICS_DEP_ROOT)/obj" LIB_FRONTEND="$(FISICS_DEP_ARCHIVE)" frontend
	@cp "$(FISICS_DEP_ARCHIVE)" "$@"

fisics-package-compiler: $(FISICS_LIB)
	@$(MAKE) -C "$(FISICS_DIR)" BUILD_PROFILE="$(FISICS_FRONTEND_BUILD_PROFILE)" CC="$(HOST_CC) $(ARCH_FLAGS)" LLVM_CONFIG="$(LLVM_CONFIG)" BUILD_DIR="$(FISICS_DEP_ROOT)/obj" LIB_FRONTEND="$(FISICS_DEP_ARCHIVE)" BIN="$(FISICS_DEP_BIN)" "$(FISICS_DEP_BIN)"
