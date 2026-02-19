# ===== CONFIGURATION =====
  CC = cc

  # Detect Homebrew prefix (works on Intel and Apple Silicon)
  BREW_PREFIX := $(shell brew --prefix 2>/dev/null)

  # Shared include/lib search paths
  VK_RENDERER_DIR := ../shared/vk_renderer
  INC_DIRS := -I./src -I./include -I$(VK_RENDERER_DIR)/include
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
  FISICS_LIB := $(FISICS_DIR)/libfisics_frontend.a
  ifeq ($(wildcard $(FISICS_LIB)),)
  $(warning Fisics frontend library not found at $(FISICS_LIB); build may fail until it is built.)
  endif

  VULKAN_RENDER_DEBUG ?= 0
  VULKAN_RENDER_DEBUG_FRAMES ?= 0

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

  CFLAGS  = -g -Wall -std=c99 -MMD -MP $(INC_DIRS) -I$(FISICS_INC) $(LLVM_CFLAGS) -DVK_RENDERER_SHADER_ROOT=\"$(ABS_VK_SHADER_ROOT)\" $(TIMER_HUD_INC)

  ifeq ($(strip $(VULKAN_RENDER_DEBUG)),1)
  CFLAGS += -DVK_RENDERER_DEBUG=1
  ifeq ($(strip $(VULKAN_RENDER_DEBUG_FRAMES)),1)
  CFLAGS += -DVK_RENDERER_FRAME_DEBUG=1
  endif
  endif
  LDFLAGS = $(LIB_DIRS) -lSDL2 -lSDL2_ttf -lSDL2_image -ljson-c $(VULKAN_LIBS) $(SDL_MIXER_FLAGS) $(FISICS_LIB) $(LLVM_LDFLAGS) $(LLVM_LIBS) -fsanitize=address,undefined

  SRC_DIR := src
  BUILD_DIR := build
  EXCLUDE_DIRS := $(SRC_DIR)/Project $(SRC_DIR)/engine/Render/vk_renderer_ref_backup $(SRC_DIR)/engine/TimerHUD_legacy_backup
  EXCLUDE_FILES :=

  SRC_FILES := $(shell find $(SRC_DIR) -type f -name '*.c' $(foreach dir,$(EXCLUDE_DIRS),! -path "$(dir)/*"))
  SRC_FILES := $(filter-out $(EXCLUDE_FILES), $(SRC_FILES))
  VK_RENDERER_SRCS := $(shell find $(VK_RENDERER_DIR)/src -type f -name '*.c')
  TIMER_HUD_SRCS := $(shell find $(TIMER_HUD_DIR)/src -type f -name '*.c')
  TIMER_HUD_EXTERNAL_SRCS := $(TIMER_HUD_DIR)/external/cJSON.c
  VK_RENDERER_OBJS := $(patsubst $(VK_RENDERER_DIR)/src/%.c,$(BUILD_DIR)/vk_renderer/%.o,$(VK_RENDERER_SRCS))
  TIMER_HUD_OBJS := $(patsubst $(TIMER_HUD_DIR)/src/%.c,$(BUILD_DIR)/timer_hud/%.o,$(TIMER_HUD_SRCS))
  TIMER_HUD_EXTERNAL_OBJS := $(patsubst $(TIMER_HUD_DIR)/external/%.c,$(BUILD_DIR)/timer_hud_external/%.o,$(TIMER_HUD_EXTERNAL_SRCS))

  OBJ_FILES := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC_FILES)) $(VK_RENDERER_OBJS) $(TIMER_HUD_OBJS) $(TIMER_HUD_EXTERNAL_OBJS)
  DEP_FILES := $(OBJ_FILES:.o=.d)

  OUT = ide
  TEST_BUILD_DIR := $(BUILD_DIR)/tests
  VK_MACRO_TEST_OBJ := $(TEST_BUILD_DIR)/vk_renderer_macro_check.o
# ===== RULES =====
all: $(OUT)

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

run: $(OUT)
	@./$(OUT)

clean:
	@rm -rf $(BUILD_DIR) $(OUT)
	@echo "Cleaned up build artifacts."

$(FISICS_LIB):
	@echo "Building Fisics frontend library..."
	@$(MAKE) -C $(FISICS_DIR) libfisics_frontend.a

.PHONY: test-vk-macros
test-vk-macros:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling Vulkan SDL macro compatibility check..."
	@$(CC) $(CFLAGS) -c tests/vk_renderer_macro_check.c -o $(VK_MACRO_TEST_OBJ) || (echo "Vulkan macro compatibility check failed."; exit 1)
	@echo "Vulkan SDL macro compatibility check passed."

-include $(DEP_FILES)
