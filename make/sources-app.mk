EXCLUDE_DIRS := $(SRC_DIR)/Project $(SRC_DIR)/engine/Render/vk_renderer_ref_backup $(SRC_DIR)/engine/TimerHUD_legacy_backup
EXCLUDE_FILES :=

APP_SRC_FILES := $(shell find $(SRC_DIR) -type f -name '*.c' $(foreach dir,$(EXCLUDE_DIRS),! -path "$(dir)/*"))
APP_SRC_FILES := $(filter-out $(EXCLUDE_FILES), $(APP_SRC_FILES))
APP_OBJ_FILES := $(patsubst $(SRC_DIR)/%.c,$(APP_OBJ_DIR)/%.o,$(APP_SRC_FILES))
TIMER_HUD_SRCS := $(shell find $(TIMER_HUD_DIR)/src -type f -name '*.c')
TIMER_HUD_EXTERNAL_SRCS := $(TIMER_HUD_DIR)/external/cJSON.c
TIMER_HUD_OBJS := $(patsubst $(TIMER_HUD_DIR)/src/%.c,$(HOST_OBJ_DIR)/timer_hud/%.o,$(TIMER_HUD_SRCS))
TIMER_HUD_EXTERNAL_OBJS := $(patsubst $(TIMER_HUD_DIR)/external/%.c,$(HOST_OBJ_DIR)/timer_hud_external/%.o,$(TIMER_HUD_EXTERNAL_SRCS))
IDEBRIDGE_SUPPORT_SRCS := src/core/Diagnostics/diagnostics_pack_export.c src/core/Diagnostics/diagnostics_core_data_export.c
IDEBRIDGE_SUPPORT_OBJS := $(patsubst src/%.c,$(HOST_OBJ_DIR)/idebridge_support/%.o,$(IDEBRIDGE_SUPPORT_SRCS))
APP_DEP_FILES := $(APP_OBJ_FILES:.o=.d)
TIMER_HUD_DEP_FILES := $(TIMER_HUD_OBJS:.o=.d) $(TIMER_HUD_EXTERNAL_OBJS:.o=.d)
IDEBRIDGE_SUPPORT_DEP_FILES := $(IDEBRIDGE_SUPPORT_OBJS:.o=.d)

# Optional shared SDL event/window adapters; compiled unchanged from the pin.
UI_SDL_ADAPTER_SRCS := $(KIT_UI_DIR)/src/kit_ui_native_text_sdl.c $(KIT_UI_DIR)/src/kit_ui_interaction_sdl.c $(KIT_UI_DIR)/src/kit_ui_window_sdl.c $(KIT_UI_DIR)/src/kit_ui_window_probe_sdl.c
UI_SDL_ADAPTER_OBJS := $(patsubst $(KIT_UI_DIR)/src/%.c,$(HOST_OBJ_DIR)/ui_sdl_adapters/%.o,$(UI_SDL_ADAPTER_SRCS))
APP_OBJ_FILES += $(UI_SDL_ADAPTER_OBJS)
APP_DEP_FILES += $(UI_SDL_ADAPTER_OBJS:.o=.d)

PANE_COMPOSITION_SRC := $(KIT_UI_DIR)/../kit_pane/src/kit_pane_composition.c
PANE_COMPOSITION_OBJ := $(HOST_OBJ_DIR)/pane_adapters/kit_pane_composition.o
APP_OBJ_FILES += $(PANE_COMPOSITION_OBJ)
APP_DEP_FILES += $(PANE_COMPOSITION_OBJ:.o=.d)

ifeq ($(shell uname -s),Darwin)
VULKAN_LIBS += -framework CoreText -framework CoreGraphics -framework CoreFoundation
endif
