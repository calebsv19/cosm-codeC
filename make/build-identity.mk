# Bind compiled consumer inputs without changing the accepted shared implementation.
.DEFAULT_GOAL := all
KIT_PANE_DIR := $(KIT_UI_DIR)/../kit_pane
L4_STAMP := $(BUILD_DIR)/l4-identity.stamp
L4_RULES = $(filter-out %.d,$(MAKEFILE_LIST))
.PHONY: l4-force shared-input-check
l4-force:
shared-input-check:
	@python3 tools/verify_shared_adoption.py --manifest config/shared_adoption.json --module "vk_renderer=$(VK_RENDERER_DIR)" --module "vk_runtime=$(VK_RUNTIME_DIR)" --module "core/core_base=$(CORE_BASE_DIR)" --module "core/core_io=$(CORE_IO_DIR)" --module "core/core_data=$(CORE_DATA_DIR)" --module "core/core_pack=$(CORE_PACK_DIR)" --module "core/core_pane_snapshot=$(CORE_PANE_SNAPSHOT_DIR)" --module "core/core_pane_module=$(CORE_PANE_MODULE_DIR)" --module "core/core_pane=$(CORE_PANE_DIR)" --module "core/core_viewport2d=$(CORE_VIEWPORT2D_DIR)" --module "core/core_theme=$(CORE_THEME_DIR)" --module "core/core_font=$(CORE_FONT_DIR)" --module "core/core_time=$(CORE_TIME_DIR)" --module "core/core_queue=$(CORE_QUEUE_DIR)" --module "core/core_sched=$(CORE_SCHED_DIR)" --module "core/core_jobs=$(CORE_JOBS_DIR)" --module "core/core_workers=$(CORE_WORKERS_DIR)" --module "core/core_wake=$(CORE_WAKE_DIR)" --module "core/core_kernel=$(CORE_KERNEL_DIR)" --module "core/core_workspace_authoring_session=$(CORE_WORKSPACE_AUTHORING_SESSION_DIR)" --module "kit/kit_render=$(KIT_RENDER_DIR)" --module "kit/kit_ui=$(KIT_UI_DIR)" --module "kit/kit_graph_struct=$(KIT_GRAPH_STRUCT_DIR)" --module "kit/kit_workspace_authoring=$(KIT_WORKSPACE_AUTHORING_DIR)" --module "timer_hud=$(TIMER_HUD_DIR)" --module "kit/kit_pane=$(KIT_PANE_DIR)"
$(L4_STAMP): l4-force | shared-input-check
	@mkdir -p "$(@D)"
	@{ printf '%s\n' '$(APP_CC)' '$(HOST_CC)' '$(CC)' '$(CPPFLAGS)' '$(CFLAGS)' '$(FISICS_APP_CFLAGS)' '$(LDFLAGS)' '$(IDEBRIDGE_LDFLAGS)' '$(ARCH_FLAGS)' '$(SHARED_CC)' '$(PKG_CONFIG)' '$(TARGET_PKG_CONFIG_LIBDIR)' '$(FISICS_DIR)' '$(LLVM_CONFIG)' '$(L4_DIRS)'; $(HOST_CC) --version; $(APP_CC) --version; cksum "$$(command -v $(firstword $(HOST_CC)))" "$$(command -v $(firstword $(APP_CC)))" $(L4_RULES) tools/shared_compile.py tools/fisics_compile.py tools/verify_shared_adoption.py config/shared_adoption.json make/fisics-snapshot-inputs.json $(TOOLCHAIN_DEP); } > "$@.tmp"
	@cmp -s "$@.tmp" "$@" 2>/dev/null || mv "$@.tmp" "$@"
	@rm -f "$@.tmp"
L4_DIRS := $(VK_RENDERER_DIR) $(VK_RUNTIME_DIR) $(CORE_BASE_DIR) $(CORE_IO_DIR) $(CORE_DATA_DIR) $(CORE_PACK_DIR) $(CORE_PANE_SNAPSHOT_DIR) $(CORE_PANE_MODULE_DIR) $(CORE_PANE_DIR) $(CORE_VIEWPORT2D_DIR) $(CORE_THEME_DIR) $(CORE_FONT_DIR) $(CORE_TIME_DIR) $(CORE_QUEUE_DIR) $(CORE_SCHED_DIR) $(CORE_JOBS_DIR) $(CORE_WORKERS_DIR) $(CORE_WAKE_DIR) $(CORE_KERNEL_DIR) $(CORE_WORKSPACE_AUTHORING_SESSION_DIR) $(KIT_RENDER_DIR) $(KIT_UI_DIR) $(KIT_GRAPH_STRUCT_DIR) $(KIT_WORKSPACE_AUTHORING_DIR) $(TIMER_HUD_DIR) $(KIT_PANE_DIR)
L4_OBJECTS := $(APP_OBJ_FILES) $(TIMER_HUD_OBJS) $(TIMER_HUD_EXTERNAL_OBJS) $(IDEBRIDGE_SUPPORT_OBJS) $(IDEBRIDGE_OBJS)
$(L4_OBJECTS) $(IDE_SHARED_LIBS) $(OUT) $(IDEBRIDGE_OUT): $(L4_STAMP) | shared-input-check
# Actual compiler-read headers include cross-module and SDK dependencies.
-include $(wildcard $(SHARED_BUILD_DIR)/*.deps/*.d)
L4_SHARED_INPUTS := $(foreach dir,$(L4_DIRS),$(shell find $(dir)/src $(dir)/include $(dir)/external -type f 2>/dev/null) $(wildcard $(dir)/Makefile $(dir)/VERSION))
$(IDE_SHARED_LIBS): $(L4_SHARED_INPUTS)
