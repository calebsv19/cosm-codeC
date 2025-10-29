# ===== CONFIGURATION =====
  CC = cc

  # Detect Homebrew prefix (works on Intel and Apple Silicon)
  BREW_PREFIX := $(shell brew --prefix 2>/dev/null)

  # Shared include/lib search paths
  INC_DIRS := -I./src -I./include
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

  ABS_VK_SHADER_ROOT := $(abspath src/engine/Render/vk_renderer_ref)

  VULKAN_RENDER_DEBUG ?= 0
  VULKAN_RENDER_DEBUG_FRAMES ?= 0

  CFLAGS  = -g -Wall -std=c99 -MMD -MP $(INC_DIRS) -DVK_RENDERER_SHADER_ROOT=\"$(ABS_VK_SHADER_ROOT)\"

  ifeq ($(strip $(VULKAN_RENDER_DEBUG)),1)
  CFLAGS += -DVK_RENDERER_DEBUG=1
  ifeq ($(strip $(VULKAN_RENDER_DEBUG_FRAMES)),1)
  CFLAGS += -DVK_RENDERER_FRAME_DEBUG=1
  endif
  endif
  LDFLAGS = $(LIB_DIRS) -lSDL2 -lSDL2_ttf -lSDL2_image -ljson-c $(VULKAN_LIBS) $(SDL_MIXER_FLAGS)

  SRC_DIR := src
  BUILD_DIR := build
  EXCLUDE_DIRS := $(SRC_DIR)/Project
  EXCLUDE_FILES := $(SRC_DIR)/engine/Render/vk_renderer_ref/main.c

  SRC_FILES := $(shell find $(SRC_DIR) -type f -name '*.c' $(foreach dir,$(EXCLUDE_DIRS),! -path "$(dir)/*"))
  SRC_FILES := $(filter-out $(EXCLUDE_FILES), $(SRC_FILES))
  OBJ_FILES := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC_FILES))
  DEP_FILES := $(OBJ_FILES:.o=.d)

  OUT = ide
  TEST_BUILD_DIR := $(BUILD_DIR)/tests
  VK_MACRO_TEST_OBJ := $(TEST_BUILD_DIR)/vk_renderer_macro_check.o
# ===== RULES =====
all: $(OUT)

$(OUT): $(OBJ_FILES)
	@echo "Linking executable..."
	@echo "LDFLAGS: $(LDFLAGS)"
	@$(CC) -o $@ $^ $(LDFLAGS) || (echo "Linking failed!" && exit 1)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@echo "CFLAGS: $(CFLAGS)"
	@$(CC) $(CFLAGS) -c $< -o $@ || (echo "Compile failed for $<" && exit 1)

run: $(OUT)
	@./$(OUT)

clean:
	@rm -rf $(BUILD_DIR) $(OUT)
	@echo "Cleaned up build artifacts."

.PHONY: test-vk-macros
test-vk-macros:
	@mkdir -p $(TEST_BUILD_DIR)
	@echo "Compiling Vulkan SDL macro compatibility check..."
	@$(CC) $(CFLAGS) -c tests/vk_renderer_macro_check.c -o $(VK_MACRO_TEST_OBJ) || (echo "Vulkan macro compatibility check failed."; exit 1)
	@echo "Vulkan SDL macro compatibility check passed."

-include $(DEP_FILES)
