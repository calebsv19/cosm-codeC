# ===== CONFIGURATION =====
  CC = cc

  # Detect Homebrew prefix (works on Intel and Apple Silicon)
  BREW_PREFIX := $(shell brew --prefix 2>/dev/null)

  # Shared include/lib search paths
  VK_RENDERER_DIR := ../shared/vk_renderer
  CORE_BASE_DIR := ../shared/core/core_base
  CORE_IO_DIR := ../shared/core/core_io
  CORE_DATA_DIR := ../shared/core/core_data
  CORE_PACK_DIR := ../shared/core/core_pack
  CORE_THEME_DIR := ../shared/core/core_theme
  CORE_FONT_DIR := ../shared/core/core_font
  CORE_TIME_DIR := ../shared/core/core_time
  CORE_QUEUE_DIR := ../shared/core/core_queue
  CORE_SCHED_DIR := ../shared/core/core_sched
  CORE_JOBS_DIR := ../shared/core/core_jobs
  CORE_WORKERS_DIR := ../shared/core/core_workers
  CORE_WAKE_DIR := ../shared/core/core_wake
  CORE_KERNEL_DIR := ../shared/core/core_kernel
  INC_DIRS := -I./src -I./include -I$(VK_RENDERER_DIR)/include -I$(CORE_BASE_DIR)/include -I$(CORE_IO_DIR)/include -I$(CORE_DATA_DIR)/include -I$(CORE_PACK_DIR)/include -I$(CORE_THEME_DIR)/include -I$(CORE_FONT_DIR)/include -I$(CORE_TIME_DIR)/include -I$(CORE_QUEUE_DIR)/include -I$(CORE_SCHED_DIR)/include -I$(CORE_JOBS_DIR)/include -I$(CORE_WORKERS_DIR)/include -I$(CORE_WAKE_DIR)/include -I$(CORE_KERNEL_DIR)/include
  LIB_DIRS :=

  ifneq ($(strip $(BREW_PREFIX)),)
  INC_DIRS += -I$(BREW_PREFIX)/include
  LIB_DIRS += -L$(BREW_PREFIX)/lib
  endif

  # Fallbacks for systems without Homebrew or with alternate prefixes
  INC_DIRS += -I/usr/local/include
  LIB_DIRS += -L/usr/local/lib

  # Allow consuming an installed Vulkan SDK
  ifneq ($(strip $(VULKAN_SDK)),)
  INC_DIRS += -I$(VULKAN_SDK)/include
  LIB_DIRS += -L$(VULKAN_SDK)/lib
  endif

  VULKAN_BREW_PREFIX := $(shell brew --prefix vulkan-loader 2>/dev/null)
  ifneq ($(strip $(VULKAN_BREW_PREFIX)),)
  INC_DIRS += -I$(VULKAN_BREW_PREFIX)/include
  LIB_DIRS += -L$(VULKAN_BREW_PREFIX)/lib
  endif

  SDL_MIXER_SEARCH := $(foreach dir,$(BREW_PREFIX)/lib /usr/local/lib /opt/homebrew/lib,$(wildcard $(dir)/libSDL2_mixer*.dylib) $(wildcard  $(dir)/libSDL2_mixer*.a))
  SDL_MIXER_LIB := $(firstword $(SDL_MIXER_SEARCH))

  ifneq ($(strip $(SDL_MIXER_LIB)),)
  SDL_MIXER_FLAGS := -lSDL2_mixer
  else
  SDL_MIXER_FLAGS :=
  endif

  VULKAN_LIBS := -lvulkan

  ABS_VK_SHADER_ROOT := $(abspath $(VK_RENDERER_DIR))

  # Fisics frontend (compiler) integration
  FISICS_DIR := ../fisiCs
  FISICS_INC := $(FISICS_DIR)/src
  FISICS_LIB_UNSANITIZED := $(FISICS_DIR)/libfisics_frontend_unsanitized.a
  FISICS_LIB_SANITIZED := $(FISICS_DIR)/libfisics_frontend_sanitized.a

  VULKAN_RENDER_DEBUG ?= 0
  VULKAN_RENDER_DEBUG_FRAMES ?= 0
  BUILD_PROFILE ?= debug
  FISICS_SANITIZED ?= 0
  ifeq ($(FISICS_SANITIZED),1)
  FISICS_LIB := $(FISICS_LIB_SANITIZED)
  FISICS_FRONTEND_TARGET := frontend-sanitized
  else
  FISICS_LIB := $(FISICS_LIB_UNSANITIZED)
  FISICS_FRONTEND_TARGET := frontend-unsanitized
  endif
  ifeq ($(wildcard $(FISICS_LIB)),)
  $(warning Fisics frontend library not found at $(FISICS_LIB); build may fail until it is built.)
  endif

  # LLVM (for Fisics frontend)
  LLVM_CONFIG := $(shell command -v llvm-config 2>/dev/null)
  LLVM_CFLAGS := $(if $(LLVM_CONFIG),$(shell $(LLVM_CONFIG) --cflags),)
  LLVM_LDFLAGS := $(if $(LLVM_CONFIG),$(shell $(LLVM_CONFIG) --ldflags),)
  LLVM_LIBS := $(if $(LLVM_CONFIG),$(shell $(LLVM_CONFIG) --libs core),)
  ifeq ($(strip $(LLVM_CONFIG)),)
  $(warning llvm-config not found; Fisics frontend linking may fail.)
  endif

  TIMER_HUD_DIR := ../shared/timer_hud
  TIMER_HUD_INC := -I$(TIMER_HUD_DIR)/include -I$(TIMER_HUD_DIR)/external

  BASE_CFLAGS = -Wall -std=c99 -MMD -MP $(INC_DIRS) -I$(FISICS_INC) $(LLVM_CFLAGS) -DVK_RENDERER_SHADER_ROOT=\"$(ABS_VK_SHADER_ROOT)\" $(TIMER_HUD_INC)
  BASE_LDFLAGS = $(LIB_DIRS) -lSDL2 -lSDL2_ttf -lSDL2_image -ljson-c $(VULKAN_LIBS) $(SDL_MIXER_FLAGS) $(FISICS_LIB) $(LLVM_LDFLAGS) $(LLVM_LIBS)

  ifeq ($(BUILD_PROFILE),debug)
  PROFILE_CFLAGS = -g -O0
  PROFILE_LDFLAGS = -fsanitize=address,undefined
  else ifeq ($(BUILD_PROFILE),perf)
  PROFILE_CFLAGS = -O3 -DNDEBUG
  ifeq ($(FISICS_SANITIZED),1)
  PROFILE_LDFLAGS = -fsanitize=address,undefined
  else
  PROFILE_LDFLAGS =
  endif
  else
  $(error Unsupported BUILD_PROFILE '$(BUILD_PROFILE)' (expected debug or perf))
  endif

  CFLAGS  = $(BASE_CFLAGS) $(PROFILE_CFLAGS)

  ifeq ($(strip $(VULKAN_RENDER_DEBUG)),1)
  CFLAGS += -DVK_RENDERER_DEBUG=1
  ifeq ($(strip $(VULKAN_RENDER_DEBUG_FRAMES)),1)
  CFLAGS += -DVK_RENDERER_FRAME_DEBUG=1
  endif
  endif
  LDFLAGS = $(BASE_LDFLAGS) $(PROFILE_LDFLAGS)

  SRC_DIR := src
  BUILD_DIR := build/$(BUILD_PROFILE)
  EXCLUDE_DIRS := $(SRC_DIR)/Project $(SRC_DIR)/engine/Render/vk_renderer_ref_backup $(SRC_DIR)/engine/TimerHUD_legacy_backup
  EXCLUDE_FILES :=

  SRC_FILES := $(shell find $(SRC_DIR) -type f -name '*.c' $(foreach dir,$(EXCLUDE_DIRS),! -path "$(dir)/*"))
  SRC_FILES := $(filter-out $(EXCLUDE_FILES), $(SRC_FILES))
  VK_RENDERER_SRCS := $(shell find $(VK_RENDERER_DIR)/src -type f -name '*.c')
  TIMER_HUD_SRCS := $(shell find $(TIMER_HUD_DIR)/src -type f -name '*.c')
  TIMER_HUD_EXTERNAL_SRCS := $(TIMER_HUD_DIR)/external/cJSON.c
  CORE_BASE_SRCS := $(CORE_BASE_DIR)/src/core_base.c
  CORE_IO_SRCS := $(CORE_IO_DIR)/src/core_io.c
  CORE_DATA_SRCS := $(CORE_DATA_DIR)/src/core_data.c
  CORE_PACK_SRCS := $(CORE_PACK_DIR)/src/core_pack.c
  CORE_THEME_SRCS := $(CORE_THEME_DIR)/src/core_theme.c
  CORE_FONT_SRCS := $(CORE_FONT_DIR)/src/core_font.c
  CORE_TIME_SRCS := $(CORE_TIME_DIR)/src/core_time.c
  ifeq ($(shell uname -s),Darwin)
  CORE_TIME_SRCS += $(CORE_TIME_DIR)/src/core_time_mac.c
  else
  CORE_TIME_SRCS += $(CORE_TIME_DIR)/src/core_time_posix.c
  endif
  CORE_QUEUE_SRCS := $(CORE_QUEUE_DIR)/src/core_queue.c
  CORE_SCHED_SRCS := $(CORE_SCHED_DIR)/src/core_sched.c
  CORE_JOBS_SRCS := $(CORE_JOBS_DIR)/src/core_jobs.c
  CORE_WORKERS_SRCS := $(CORE_WORKERS_DIR)/src/core_workers.c
  CORE_WAKE_SRCS := $(CORE_WAKE_DIR)/src/core_wake.c
  CORE_KERNEL_SRCS := $(CORE_KERNEL_DIR)/src/core_kernel.c
  VK_RENDERER_OBJS := $(patsubst $(VK_RENDERER_DIR)/src/%.c,$(BUILD_DIR)/vk_renderer/%.o,$(VK_RENDERER_SRCS))
  TIMER_HUD_OBJS := $(patsubst $(TIMER_HUD_DIR)/src/%.c,$(BUILD_DIR)/timer_hud/%.o,$(TIMER_HUD_SRCS))
  TIMER_HUD_EXTERNAL_OBJS := $(patsubst $(TIMER_HUD_DIR)/external/%.c,$(BUILD_DIR)/timer_hud_external/%.o,$(TIMER_HUD_EXTERNAL_SRCS))
  CORE_BASE_OBJS := $(patsubst $(CORE_BASE_DIR)/src/%.c,$(BUILD_DIR)/core_base/%.o,$(CORE_BASE_SRCS))
  CORE_IO_OBJS := $(patsubst $(CORE_IO_DIR)/src/%.c,$(BUILD_DIR)/core_io/%.o,$(CORE_IO_SRCS))
  CORE_DATA_OBJS := $(patsubst $(CORE_DATA_DIR)/src/%.c,$(BUILD_DIR)/core_data/%.o,$(CORE_DATA_SRCS))
  CORE_PACK_OBJS := $(patsubst $(CORE_PACK_DIR)/src/%.c,$(BUILD_DIR)/core_pack/%.o,$(CORE_PACK_SRCS))
  CORE_THEME_OBJS := $(patsubst $(CORE_THEME_DIR)/src/%.c,$(BUILD_DIR)/core_theme/%.o,$(CORE_THEME_SRCS))
  CORE_FONT_OBJS := $(patsubst $(CORE_FONT_DIR)/src/%.c,$(BUILD_DIR)/core_font/%.o,$(CORE_FONT_SRCS))
  CORE_TIME_OBJS := $(patsubst $(CORE_TIME_DIR)/src/%.c,$(BUILD_DIR)/core_time/%.o,$(CORE_TIME_SRCS))
  CORE_QUEUE_OBJS := $(patsubst $(CORE_QUEUE_DIR)/src/%.c,$(BUILD_DIR)/core_queue/%.o,$(CORE_QUEUE_SRCS))
  CORE_SCHED_OBJS := $(patsubst $(CORE_SCHED_DIR)/src/%.c,$(BUILD_DIR)/core_sched/%.o,$(CORE_SCHED_SRCS))
  CORE_JOBS_OBJS := $(patsubst $(CORE_JOBS_DIR)/src/%.c,$(BUILD_DIR)/core_jobs/%.o,$(CORE_JOBS_SRCS))
  CORE_WORKERS_OBJS := $(patsubst $(CORE_WORKERS_DIR)/src/%.c,$(BUILD_DIR)/core_workers/%.o,$(CORE_WORKERS_SRCS))
  CORE_WAKE_OBJS := $(patsubst $(CORE_WAKE_DIR)/src/%.c,$(BUILD_DIR)/core_wake/%.o,$(CORE_WAKE_SRCS))
  CORE_KERNEL_OBJS := $(patsubst $(CORE_KERNEL_DIR)/src/%.c,$(BUILD_DIR)/core_kernel/%.o,$(CORE_KERNEL_SRCS))

  OBJ_FILES := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC_FILES)) $(VK_RENDERER_OBJS) $(TIMER_HUD_OBJS) $(TIMER_HUD_EXTERNAL_OBJS) $(CORE_BASE_OBJS) $(CORE_IO_OBJS) $(CORE_DATA_OBJS) $(CORE_PACK_OBJS) $(CORE_THEME_OBJS) $(CORE_FONT_OBJS) $(CORE_TIME_OBJS) $(CORE_QUEUE_OBJS) $(CORE_SCHED_OBJS) $(CORE_JOBS_OBJS) $(CORE_WORKERS_OBJS) $(CORE_WAKE_OBJS) $(CORE_KERNEL_OBJS)
  DEP_FILES := $(OBJ_FILES:.o=.d)

  OUT = ide
  IDEBRIDGE_OUT = idebridge
  IDEBRIDGE_SRC := tools/idebridge/idebridge.c
  IDEBRIDGE_OBJ := $(BUILD_DIR)/tools/idebridge.o
  DIAG_PACK_EXPORT_OBJ := $(BUILD_DIR)/core/Diagnostics/diagnostics_pack_export.o
  DIAG_DATA_EXPORT_OBJ := $(BUILD_DIR)/core/Diagnostics/diagnostics_core_data_export.o
  IDEBRIDGE_LDFLAGS := $(LIB_DIRS) -ljson-c
  TEST_BUILD_DIR := $(BUILD_DIR)/tests
  VK_MACRO_TEST_OBJ := $(TEST_BUILD_DIR)/vk_renderer_macro_check.o
  IDEBRIDGE_PHASE1_TEST_OUT := $(TEST_BUILD_DIR)/idebridge_phase1_check
  IDEBRIDGE_PHASE2_TEST_OUT := $(TEST_BUILD_DIR)/idebridge_phase2_check
  IDEBRIDGE_PHASE3_TEST_OUT := $(TEST_BUILD_DIR)/idebridge_phase3_check
  IDEBRIDGE_PHASE4_TEST_OUT := $(TEST_BUILD_DIR)/idebridge_phase4_check
  IDEBRIDGE_PHASE5_TEST_OUT := $(TEST_BUILD_DIR)/idebridge_phase5_check
  IDEBRIDGE_PHASE6_TEST_OUT := $(TEST_BUILD_DIR)/idebridge_phase6_check
# ===== RULES =====
all: $(OUT) $(IDEBRIDGE_OUT)

.PHONY: debug perf run-debug run-perf run-perf-log run-perf-hud run-perf-nohud run-perf-sanitized
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

$(OUT): $(OBJ_FILES) $(FISICS_LIB)
	@echo "Linking executable..."
	@echo "LDFLAGS: $(LDFLAGS)"
	@$(CC) -o $@ $^ $(LDFLAGS) || (echo "Linking failed!" && exit 1)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@echo "CFLAGS: $(CFLAGS)"
	@$(CC) $(CFLAGS) -c $< -o $@ || (echo "Compile failed for $<" && exit 1)

$(BUILD_DIR)/timer_hud/%.o: $(TIMER_HUD_DIR)/src/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@echo "CFLAGS: $(CFLAGS)"
	@$(CC) $(CFLAGS) -c $< -o $@ || (echo "Compile failed for $<" && exit 1)

$(BUILD_DIR)/vk_renderer/%.o: $(VK_RENDERER_DIR)/src/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@echo "CFLAGS: $(CFLAGS)"
	@$(CC) $(CFLAGS) -c $< -o $@ || (echo "Compile failed for $<" && exit 1)

$(BUILD_DIR)/timer_hud_external/%.o: $(TIMER_HUD_DIR)/external/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@echo "CFLAGS: $(CFLAGS)"
	@$(CC) $(CFLAGS) -c $< -o $@ || (echo "Compile failed for $<" && exit 1)

$(BUILD_DIR)/core_base/%.o: $(CORE_BASE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@echo "CFLAGS: $(CFLAGS)"
	@$(CC) $(CFLAGS) -c $< -o $@ || (echo "Compile failed for $<" && exit 1)

$(BUILD_DIR)/core_io/%.o: $(CORE_IO_DIR)/src/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@echo "CFLAGS: $(CFLAGS)"
	@$(CC) $(CFLAGS) -c $< -o $@ || (echo "Compile failed for $<" && exit 1)

$(BUILD_DIR)/core_data/%.o: $(CORE_DATA_DIR)/src/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@echo "CFLAGS: $(CFLAGS)"
	@$(CC) $(CFLAGS) -c $< -o $@ || (echo "Compile failed for $<" && exit 1)

$(BUILD_DIR)/core_pack/%.o: $(CORE_PACK_DIR)/src/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@echo "CFLAGS: $(CFLAGS)"
	@$(CC) $(CFLAGS) -c $< -o $@ || (echo "Compile failed for $<" && exit 1)

$(BUILD_DIR)/core_theme/%.o: $(CORE_THEME_DIR)/src/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@echo "CFLAGS: $(CFLAGS)"
	@$(CC) $(CFLAGS) -c $< -o $@ || (echo "Compile failed for $<" && exit 1)

$(BUILD_DIR)/core_font/%.o: $(CORE_FONT_DIR)/src/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@echo "CFLAGS: $(CFLAGS)"
	@$(CC) $(CFLAGS) -c $< -o $@ || (echo "Compile failed for $<" && exit 1)

$(BUILD_DIR)/core_time/%.o: $(CORE_TIME_DIR)/src/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@echo "CFLAGS: $(CFLAGS)"
	@$(CC) $(CFLAGS) -c $< -o $@ || (echo "Compile failed for $<" && exit 1)

$(BUILD_DIR)/core_queue/%.o: $(CORE_QUEUE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@echo "CFLAGS: $(CFLAGS)"
	@$(CC) $(CFLAGS) -c $< -o $@ || (echo "Compile failed for $<" && exit 1)

$(BUILD_DIR)/core_sched/%.o: $(CORE_SCHED_DIR)/src/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@echo "CFLAGS: $(CFLAGS)"
	@$(CC) $(CFLAGS) -c $< -o $@ || (echo "Compile failed for $<" && exit 1)

$(BUILD_DIR)/core_jobs/%.o: $(CORE_JOBS_DIR)/src/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@echo "CFLAGS: $(CFLAGS)"
	@$(CC) $(CFLAGS) -c $< -o $@ || (echo "Compile failed for $<" && exit 1)

$(BUILD_DIR)/core_workers/%.o: $(CORE_WORKERS_DIR)/src/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@echo "CFLAGS: $(CFLAGS)"
	@$(CC) $(CFLAGS) -c $< -o $@ || (echo "Compile failed for $<" && exit 1)

$(BUILD_DIR)/core_wake/%.o: $(CORE_WAKE_DIR)/src/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@echo "CFLAGS: $(CFLAGS)"
	@$(CC) $(CFLAGS) -c $< -o $@ || (echo "Compile failed for $<" && exit 1)

$(BUILD_DIR)/core_kernel/%.o: $(CORE_KERNEL_DIR)/src/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@echo "CFLAGS: $(CFLAGS)"
	@$(CC) $(CFLAGS) -c $< -o $@ || (echo "Compile failed for $<" && exit 1)

run: run-ide-theme

.PHONY: run-ide-theme
run-ide-theme: $(OUT)
	@MallocNanoZone=0 IDE_USE_SHARED_THEME_FONT=1 IDE_USE_SHARED_THEME=1 IDE_USE_SHARED_FONT=1 IDE_THEME_PRESET=ide_gray IDE_FONT_PRESET=ide IDE_TIMER_HUD=0 ./$(OUT)

.PHONY: run-ide-theme-log
run-ide-theme-log: $(OUT)
	@MallocNanoZone=0 IDE_USE_SHARED_THEME_FONT=1 IDE_USE_SHARED_THEME=1 IDE_USE_SHARED_FONT=1 IDE_THEME_PRESET=ide_gray IDE_FONT_PRESET=ide IDE_TIMER_HUD=1 IDE_TIMER_HUD_OVERLAY=0 ./$(OUT)

.PHONY: run-ide-theme-hud
run-ide-theme-hud: $(OUT)
	@MallocNanoZone=0 IDE_USE_SHARED_THEME_FONT=1 IDE_USE_SHARED_THEME=1 IDE_USE_SHARED_FONT=1 IDE_THEME_PRESET=ide_gray IDE_FONT_PRESET=ide IDE_TIMER_HUD=1 IDE_TIMER_HUD_OVERLAY=1 IDE_TIMER_HUD_VISUAL_MODE=hybrid ./$(OUT)

.PHONY: run-ide-theme-nohud
run-ide-theme-nohud: run-ide-theme-log

.PHONY: run-daw-theme
run-daw-theme: $(OUT)
	@IDE_USE_SHARED_THEME_FONT=1 IDE_USE_SHARED_THEME=1 IDE_USE_SHARED_FONT=1 IDE_THEME_PRESET=daw_default IDE_FONT_PRESET=daw_default ./$(OUT)

$(IDEBRIDGE_OUT): $(IDEBRIDGE_OBJ) $(DIAG_PACK_EXPORT_OBJ) $(DIAG_DATA_EXPORT_OBJ) $(CORE_PACK_OBJS) $(CORE_BASE_OBJS) $(CORE_IO_OBJS) $(CORE_DATA_OBJS)
	@echo "Linking idebridge..."
	@$(CC) -o $@ $^ $(IDEBRIDGE_LDFLAGS) || (echo "idebridge linking failed!" && exit 1)

$(BUILD_DIR)/tools/idebridge.o: $(IDEBRIDGE_SRC)
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@$(CC) $(CFLAGS) -c $< -o $@ || (echo "Compile failed for $<" && exit 1)

clean:
	@rm -rf build $(OUT) $(IDEBRIDGE_OUT)
	@echo "Cleaned up build artifacts."

$(FISICS_LIB):
	@echo "Building Fisics frontend library..."
	@$(MAKE) -C $(FISICS_DIR) $(FISICS_FRONTEND_TARGET)

.PHONY: test-vk-macros
test-vk-macros:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling Vulkan SDL macro compatibility check..."
	@$(CC) $(CFLAGS) -c tests/vk_renderer_macro_check.c -o $(VK_MACRO_TEST_OBJ) || (echo "Vulkan macro compatibility check failed."; exit 1)
	@echo "Vulkan SDL macro compatibility check passed."

.PHONY: test-idebridge-phase1
test-idebridge-phase1: $(IDEBRIDGE_OUT)
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling idebridge phase-1 runtime check..."
	@$(CC) $(CFLAGS) tests/idebridge_phase1_check.c src/core/Ipc/ide_ipc_server.c src/core/Diagnostics/diagnostics_engine.c src/core/BuildSystem/build_diagnostics.c src/core/Analysis/analysis_symbols_store.c src/core/Analysis/library_index.c src/app/GlobalInfo/workspace_prefs.c -o $(IDEBRIDGE_PHASE1_TEST_OUT) $(LIB_DIRS) -ljson-c || (echo "idebridge phase-1 compile failed."; exit 1)
	@echo "Running idebridge phase-1 runtime check..."
	@$(IDEBRIDGE_PHASE1_TEST_OUT) || (echo "idebridge phase-1 runtime check failed."; exit 1)
	@echo "idebridge phase-1 runtime check passed."

.PHONY: test-idebridge-phase2
test-idebridge-phase2:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling idebridge phase-2 runtime check..."
	@$(CC) $(CFLAGS) tests/idebridge_phase2_check.c src/core/Ipc/ide_ipc_server.c src/core/Diagnostics/diagnostics_engine.c src/core/BuildSystem/build_diagnostics.c src/core/Analysis/analysis_symbols_store.c src/core/Analysis/library_index.c src/app/GlobalInfo/workspace_prefs.c -o $(IDEBRIDGE_PHASE2_TEST_OUT) $(LIB_DIRS) -ljson-c || (echo "idebridge phase-2 compile failed."; exit 1)
	@echo "Running idebridge phase-2 runtime check..."
	@$(IDEBRIDGE_PHASE2_TEST_OUT) || (echo "idebridge phase-2 runtime check failed."; exit 1)
	@echo "idebridge phase-2 runtime check passed."

.PHONY: test-idebridge-phase3
test-idebridge-phase3:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling idebridge phase-3 runtime check..."
	@$(CC) $(CFLAGS) tests/idebridge_phase3_check.c src/core/Ipc/ide_ipc_server.c src/core/Diagnostics/diagnostics_engine.c src/core/BuildSystem/build_diagnostics.c src/core/Analysis/analysis_symbols_store.c src/core/Analysis/library_index.c src/app/GlobalInfo/workspace_prefs.c -o $(IDEBRIDGE_PHASE3_TEST_OUT) $(LIB_DIRS) -ljson-c || (echo "idebridge phase-3 compile failed."; exit 1)
	@echo "Running idebridge phase-3 runtime check..."
	@$(IDEBRIDGE_PHASE3_TEST_OUT) || (echo "idebridge phase-3 runtime check failed."; exit 1)
	@echo "idebridge phase-3 runtime check passed."

.PHONY: test-idebridge-phase4
test-idebridge-phase4:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling idebridge phase-4 runtime check..."
	@$(CC) $(CFLAGS) tests/idebridge_phase4_check.c src/core/Ipc/ide_ipc_server.c src/core/Diagnostics/diagnostics_engine.c src/core/BuildSystem/build_diagnostics.c src/core/Analysis/analysis_symbols_store.c src/core/Analysis/library_index.c src/app/GlobalInfo/workspace_prefs.c -o $(IDEBRIDGE_PHASE4_TEST_OUT) $(LIB_DIRS) -ljson-c || (echo "idebridge phase-4 compile failed."; exit 1)
	@echo "Running idebridge phase-4 runtime check..."
	@$(IDEBRIDGE_PHASE4_TEST_OUT) || (echo "idebridge phase-4 runtime check failed."; exit 1)
	@echo "idebridge phase-4 runtime check passed."

.PHONY: test-idebridge-phase5
test-idebridge-phase5: $(IDEBRIDGE_OUT)
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling idebridge phase-5 runtime check..."
	@$(CC) $(CFLAGS) tests/idebridge_phase5_check.c src/core/Ipc/ide_ipc_server.c src/core/Diagnostics/diagnostics_engine.c src/core/BuildSystem/build_diagnostics.c src/core/Analysis/analysis_symbols_store.c src/core/Analysis/library_index.c src/app/GlobalInfo/workspace_prefs.c -o $(IDEBRIDGE_PHASE5_TEST_OUT) $(LIB_DIRS) -ljson-c || (echo "idebridge phase-5 compile failed."; exit 1)
	@echo "Running idebridge phase-5 runtime check..."
	@$(IDEBRIDGE_PHASE5_TEST_OUT) || (echo "idebridge phase-5 runtime check failed."; exit 1)
	@echo "idebridge phase-5 runtime check passed."

.PHONY: test-idebridge-phase6
test-idebridge-phase6: $(IDEBRIDGE_OUT)
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling idebridge phase-6 runtime check..."
	@$(CC) $(CFLAGS) tests/idebridge_phase6_check.c src/core/Ipc/ide_ipc_server.c src/core/Ipc/ide_ipc_path_guard.c src/core/Diagnostics/diagnostics_engine.c src/core/BuildSystem/build_diagnostics.c src/core/Analysis/analysis_symbols_store.c src/core/Analysis/library_index.c src/app/GlobalInfo/workspace_prefs.c -o $(IDEBRIDGE_PHASE6_TEST_OUT) $(LIB_DIRS) -ljson-c -lSDL2 || (echo "idebridge phase-6 compile failed."; exit 1)
	@echo "Running idebridge phase-6 runtime check..."
	@$(IDEBRIDGE_PHASE6_TEST_OUT) || (echo "idebridge phase-6 runtime check failed."; exit 1)
	@echo "idebridge phase-6 runtime check passed."

.PHONY: test-idebridge-regression
test-idebridge-regression:
	@$(MAKE) test-idebridge-phase1
	@$(MAKE) test-idebridge-phase2
	@$(MAKE) test-idebridge-phase3
	@$(MAKE) test-idebridge-phase4
	@$(MAKE) test-idebridge-phase5
	@$(MAKE) test-idebridge-phase6

.PHONY: test-shared-theme-font-adapter
test-shared-theme-font-adapter:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling IDE shared theme/font adapter test..."
	@$(CC) -std=c99 -Wall -Wextra -MMD -MP $(INC_DIRS) \
		tests/shared_theme_font_adapter_test.c \
		src/ide/UI/shared_theme_font_adapter.c \
		$(CORE_THEME_DIR)/src/core_theme.c \
		$(CORE_FONT_DIR)/src/core_font.c \
		$(CORE_BASE_DIR)/src/core_base.c \
		-o $(TEST_BUILD_DIR)/shared_theme_font_adapter_test \
		$(LIB_DIRS) -lSDL2 || (echo "shared theme/font adapter test compile failed."; exit 1)
	@echo "Running IDE shared theme/font adapter test..."
	@$(TEST_BUILD_DIR)/shared_theme_font_adapter_test || (echo "shared theme/font adapter test failed."; exit 1)
	@echo "IDE shared theme/font adapter test passed."

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

-include $(DEP_FILES)
