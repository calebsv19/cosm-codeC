all: $(OUT) $(IDEBRIDGE_OUT)

debug:
	@$(MAKE) BUILD_PROFILE=debug all

perf:
	@$(MAKE) BUILD_PROFILE=perf all

run-debug:
	@$(MAKE) BUILD_PROFILE=debug run-ide-theme

run-perf:
	@$(MAKE) BUILD_PROFILE=perf FISICS_SANITIZED=0 run-ide-theme

run-perf-log:
	@$(MAKE) BUILD_PROFILE=perf FISICS_SANITIZED=0 run-ide-theme-log

run-perf-hud:
	@$(MAKE) BUILD_PROFILE=perf FISICS_SANITIZED=0 run-ide-theme-hud

run-perf-nohud: run-perf-log

run-perf-sanitized:
	@$(MAKE) BUILD_PROFILE=perf FISICS_SANITIZED=1 run-ide-theme

FORCE:

$(SHARED_BUILD_DIR):
	@mkdir -p $@

define build_copy_static_lib
$($(1)_LIB): FORCE | $(SHARED_BUILD_DIR)
	@$(MAKE) -C $($(1)_DIR) clean $(2)
	@PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" PKG_CONFIG="$(PKG_CONFIG)" $(MAKE) -C $($(1)_DIR) CC="$(SHARED_CC)" $(2)
	@cp "$($(1)_LIB_SRC)" "$$@"
endef

$(eval $(call build_copy_static_lib,CORE_BASE,))
$(eval $(call build_copy_static_lib,CORE_IO,))
$(eval $(call build_copy_static_lib,CORE_DATA,))
$(eval $(call build_copy_static_lib,CORE_PACK,))
$(eval $(call build_copy_static_lib,CORE_PANE,))
$(eval $(call build_copy_static_lib,CORE_THEME,))
$(eval $(call build_copy_static_lib,CORE_FONT,))
$(eval $(call build_copy_static_lib,CORE_TIME,))
$(eval $(call build_copy_static_lib,CORE_QUEUE,))
$(eval $(call build_copy_static_lib,CORE_SCHED,))
$(eval $(call build_copy_static_lib,CORE_JOBS,))
$(eval $(call build_copy_static_lib,CORE_WORKERS,))
$(eval $(call build_copy_static_lib,CORE_WAKE,))
$(eval $(call build_copy_static_lib,CORE_KERNEL,))
$(eval $(call build_copy_static_lib,KIT_RENDER,KIT_RENDER_ENABLE_VK=1))
$(eval $(call build_copy_static_lib,KIT_WORKSPACE_AUTHORING,))
$(eval $(call build_copy_static_lib,VK_RENDERER,))

$(FISICS_LIB): FORCE | $(SHARED_BUILD_DIR)
	@test -n "$(LLVM_CONFIG)" || (echo "Missing target llvm-config for $(TARGET_TRIPLE); install llvm under $(TARGET_HOMEBREW_PREFIX)/opt/llvm" && exit 1)
	@$(MAKE) -C $(FISICS_DIR) BUILD_PROFILE="$(FISICS_FRONTEND_BUILD_PROFILE)" clean
	@$(MAKE) -C $(FISICS_DIR) BUILD_PROFILE="$(FISICS_FRONTEND_BUILD_PROFILE)" CC="$(HOST_CC) $(ARCH_FLAGS)" LLVM_CONFIG="$(LLVM_CONFIG)" $(FISICS_FRONTEND_TARGET)
	@cp "$(FISICS_FRONTEND_ARCHIVE_SRC)" "$@"

$(APP_BIN_DIR) $(COMPILER_STAMP_DIR):
	@mkdir -p $@

$(COMPILER_STAMP): $(TOOLCHAIN_DEP) | $(COMPILER_STAMP_DIR)
	@printf '%s\n' "$(APP_CC)" > $@

$(OUT): $(APP_OBJ_FILES) $(TIMER_HUD_OBJS) $(TIMER_HUD_EXTERNAL_OBJS) $(IDE_SHARED_LIBS) $(FISICS_LIB) | $(APP_BIN_DIR)
	@echo "Linking executable..."
	@echo "LDFLAGS: $(LDFLAGS)"
	@$(HOST_CC) $(ARCH_FLAGS) -o $@ $(APP_OBJ_FILES) $(TIMER_HUD_OBJS) $(TIMER_HUD_EXTERNAL_OBJS) $(IDE_SHARED_LIBS) $(LDFLAGS) || (echo "Linking failed!" && exit 1)

$(APP_OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(COMPILER_STAMP)
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@echo "CFLAGS: $(CFLAGS)"
	@$(APP_CC) $(CFLAGS) $(if $(filter clang,$(BUILD_TOOLCHAIN)),$(ARCH_FLAGS),) -c $< -o $@ || (echo "Compile failed for $<" && exit 1)

$(HOST_OBJ_DIR)/timer_hud/%.o: $(TIMER_HUD_DIR)/src/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@echo "CFLAGS: $(CFLAGS)"
	@$(HOST_CC) $(CFLAGS) $(ARCH_FLAGS) -c $< -o $@ || (echo "Compile failed for $<" && exit 1)

$(HOST_OBJ_DIR)/timer_hud_external/%.o: $(TIMER_HUD_DIR)/external/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@echo "CFLAGS: $(CFLAGS)"
	@$(HOST_CC) $(CFLAGS) $(ARCH_FLAGS) -c $< -o $@ || (echo "Compile failed for $<" && exit 1)

$(HOST_OBJ_DIR)/idebridge_support/%.o: src/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@echo "CFLAGS: $(CFLAGS)"
	@$(HOST_CC) $(CFLAGS) $(ARCH_FLAGS) -c $< -o $@ || (echo "Compile failed for $<" && exit 1)

$(IDEBRIDGE_OUT): $(IDEBRIDGE_OBJ) $(DIAG_PACK_EXPORT_OBJ) $(DIAG_DATA_EXPORT_OBJ) $(IDEBRIDGE_SHARED_LIBS)
	@echo "Linking idebridge..."
	@$(CC) $(ARCH_FLAGS) -o $@ $(IDEBRIDGE_OBJ) $(DIAG_PACK_EXPORT_OBJ) $(DIAG_DATA_EXPORT_OBJ) $(IDEBRIDGE_SHARED_LIBS) $(IDEBRIDGE_LDFLAGS) || (echo "idebridge linking failed!" && exit 1)

$(TOOLS_BUILD_DIR)/idebridge.o: $(IDEBRIDGE_SRC)
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@$(CC) $(CFLAGS) $(ARCH_FLAGS) -c $< -o $@ || (echo "Compile failed for $<" && exit 1)

clean:
	@rm -rf build $(OUT) $(IDEBRIDGE_OUT)
	@echo "Cleaned up build artifacts."
