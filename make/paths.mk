ifneq ($(wildcard $(TARGET_HOMEBREW_PREFIX)/include),)
INC_DIRS += -I$(TARGET_HOMEBREW_PREFIX)/include
LIB_DIRS += -L$(TARGET_HOMEBREW_PREFIX)/lib
else ifneq ($(wildcard $(TARGET_ALT_HOMEBREW_PREFIX)/include),)
INC_DIRS += -I$(TARGET_ALT_HOMEBREW_PREFIX)/include
LIB_DIRS += -L$(TARGET_ALT_HOMEBREW_PREFIX)/lib
endif

# Allow consuming an installed Vulkan SDK
ifneq ($(strip $(VULKAN_SDK)),)
INC_DIRS += -I$(VULKAN_SDK)/include
LIB_DIRS += -L$(VULKAN_SDK)/lib
endif

VULKAN_PKG_CFLAGS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --cflags vulkan 2>/dev/null)
VULKAN_PKG_LIBS := $(shell env PKG_CONFIG_LIBDIR="$(TARGET_PKG_CONFIG_LIBDIR)" $(PKG_CONFIG) --libs vulkan 2>/dev/null)
ifneq ($(strip $(VULKAN_PKG_CFLAGS)),)
INC_DIRS += $(VULKAN_PKG_CFLAGS)
endif
ifneq ($(strip $(VULKAN_PKG_LIBS)),)
LIB_DIRS += $(filter -L%,$(VULKAN_PKG_LIBS))
endif

SDL_MIXER_SEARCH := $(foreach dir,$(TARGET_HOMEBREW_PREFIX)/lib $(TARGET_ALT_HOMEBREW_PREFIX)/lib,$(wildcard $(dir)/libSDL2_mixer*.dylib) $(wildcard $(dir)/libSDL2_mixer*.a))
SDL_MIXER_LIB := $(firstword $(SDL_MIXER_SEARCH))

ifneq ($(strip $(SDL_MIXER_LIB)),)
SDL_MIXER_FLAGS := -lSDL2_mixer
else
SDL_MIXER_FLAGS :=
endif

ifneq ($(strip $(VULKAN_PKG_LIBS)),)
VULKAN_LIBS := $(filter-out -L%,$(VULKAN_PKG_LIBS))
else
VULKAN_LIBS := -lvulkan
endif

ABS_VK_SHADER_ROOT := $(abspath $(VK_RENDERER_DIR))

# Fisics frontend (compiler) integration
FISICS_DIR := ../fisiCs
FISICS_INC := $(FISICS_DIR)/src
FISICS_LEXER_INC := $(FISICS_DIR)/src/Lexer
FISICS_LIB_UNSANITIZED_SRC := $(FISICS_DIR)/libfisics_frontend_unsanitized.a
FISICS_LIB_SANITIZED_SRC := $(FISICS_DIR)/libfisics_frontend_sanitized.a

VULKAN_RENDER_DEBUG ?= 0
VULKAN_RENDER_DEBUG_FRAMES ?= 0
BUILD_PROFILE ?= debug
FISICS_SANITIZED ?= 0
ifeq ($(FISICS_SANITIZED),1)
FISICS_FRONTEND_ARCHIVE_SRC := $(FISICS_LIB_SANITIZED_SRC)
FISICS_FRONTEND_TARGET := frontend-sanitized
FISICS_FRONTEND_BUILD_PROFILE := sanitized
else
FISICS_FRONTEND_ARCHIVE_SRC := $(FISICS_LIB_UNSANITIZED_SRC)
FISICS_FRONTEND_TARGET := frontend-unsanitized
FISICS_FRONTEND_BUILD_PROFILE := unsanitized
endif
ifeq ($(wildcard $(FISICS_FRONTEND_ARCHIVE_SRC)),)
$(warning Fisics frontend library not found at $(FISICS_FRONTEND_ARCHIVE_SRC); build may fail until it is built.)
endif

# LLVM (for Fisics frontend)
TARGET_LLVM_CONFIG ?= $(if $(wildcard $(TARGET_HOMEBREW_PREFIX)/opt/llvm/bin/llvm-config),$(TARGET_HOMEBREW_PREFIX)/opt/llvm/bin/llvm-config,$(if $(filter $(TARGET_ARCH),$(HOST_ARCH)),$(if $(wildcard $(TARGET_ALT_HOMEBREW_PREFIX)/opt/llvm/bin/llvm-config),$(TARGET_ALT_HOMEBREW_PREFIX)/opt/llvm/bin/llvm-config,$(shell command -v llvm-config 2>/dev/null)),))
LLVM_CONFIG := $(TARGET_LLVM_CONFIG)
LLVM_CFLAGS := $(if $(LLVM_CONFIG),$(shell $(LLVM_CONFIG) --cflags),)
LLVM_LDFLAGS := $(if $(LLVM_CONFIG),$(shell $(LLVM_CONFIG) --ldflags),)
LLVM_LIBS := $(if $(LLVM_CONFIG),$(shell $(LLVM_CONFIG) --libs core),)
LLVM_PACKAGE_ROOT := $(if $(LLVM_CONFIG),$(abspath $(dir $(LLVM_CONFIG))/..),)
ifeq ($(strip $(LLVM_CONFIG)),)
$(warning llvm-config not found for target lane; Fisics frontend/IDE linking may fail.)
endif

TIMER_HUD_DIR := $(SHARED_ROOT)/timer_hud
TIMER_HUD_INC := -I$(TIMER_HUD_DIR)/include -I$(TIMER_HUD_DIR)/external

SRC_DIR := src
TARGET_BUILD_ROOT := build/targets/$(TARGET_TRIPLE)
BUILD_DIR := $(TARGET_BUILD_ROOT)/$(BUILD_PROFILE)
TOOLCHAIN_BUILD_ROOT := $(BUILD_DIR)/toolchains
APP_BUILD_ROOT := $(TOOLCHAIN_BUILD_ROOT)/$(BUILD_TOOLCHAIN)
APP_OBJ_DIR := $(APP_BUILD_ROOT)/app
APP_BIN_DIR := $(APP_BUILD_ROOT)/bin
COMPILER_STAMP_DIR := $(APP_BUILD_ROOT)/compiler
COMPILER_STAMP := $(COMPILER_STAMP_DIR)/$(BUILD_TOOLCHAIN).stamp
HOST_OBJ_DIR := $(BUILD_DIR)/host
TOOLS_BUILD_DIR := $(BUILD_DIR)/tools
TEST_BUILD_DIR := $(BUILD_DIR)/tests
SHARED_BUILD_DIR := $(BUILD_DIR)/shared

DIST_DIR := $(TARGET_BUILD_ROOT)/dist
PACKAGE_APP_NAME := codeC.app
PACKAGE_APP_DIR := $(DIST_DIR)/$(PACKAGE_APP_NAME)
PACKAGE_CONTENTS_DIR := $(PACKAGE_APP_DIR)/Contents
PACKAGE_MACOS_DIR := $(PACKAGE_CONTENTS_DIR)/MacOS
PACKAGE_RESOURCES_DIR := $(PACKAGE_CONTENTS_DIR)/Resources
PACKAGE_FRAMEWORKS_DIR := $(PACKAGE_CONTENTS_DIR)/Frameworks
PACKAGE_APP_ICON_NAME := AppIcon
PACKAGE_APP_ICON_FILE := $(PACKAGE_APP_ICON_NAME).icns
PACKAGE_LOCAL_ICON_DIR := tools/packaging/macos/local_app_icon
PACKAGE_APP_ICON_SRC ?= $(PACKAGE_LOCAL_ICON_DIR)/$(PACKAGE_APP_ICON_FILE)
PACKAGE_APP_ICONSET_SRC ?= $(PACKAGE_LOCAL_ICON_DIR)/$(PACKAGE_APP_ICON_NAME).iconset
PACKAGE_BUNDLED_ICON_PATH := $(PACKAGE_RESOURCES_DIR)/$(PACKAGE_APP_ICON_FILE)
PACKAGE_INFO_PLIST_SRC := tools/packaging/macos/Info.plist
PACKAGE_LAUNCHER_SRC := tools/packaging/macos/ide-launcher
PACKAGE_DYLIB_BUNDLER := tools/packaging/macos/bundle-dylibs.sh
# Keep the standard Homebrew roots when extending the bundler with LLVM. An
# empty initial value would otherwise suppress bundle-dylibs.sh's defaults and
# leave transitive @rpath dependencies (for example JPEG XL/WebP siblings)
# unresolved in the app.
PACKAGE_DEP_SEARCH_ROOTS := /opt/homebrew:/usr/local$(if $(LLVM_PACKAGE_ROOT),:$(LLVM_PACKAGE_ROOT),)
PACKAGE_REQUIRED_DYLIBS := libMoltenVK.dylib libvulkan.1.dylib libLLVM.dylib
PACKAGE_BUILD_PROFILE ?= perf
PACKAGE_BUILD_DIR := $(TARGET_BUILD_ROOT)/$(PACKAGE_BUILD_PROFILE)
PACKAGE_TOOLCHAIN_BUILD_ROOT := $(PACKAGE_BUILD_DIR)/toolchains
PACKAGE_BIN := $(PACKAGE_TOOLCHAIN_BUILD_ROOT)/$(PACKAGE_TOOLCHAIN)/bin/ide
PACKAGE_IDEBRIDGE_BIN := $(PACKAGE_BUILD_DIR)/tools/idebridge
PACKAGE_FISICS_BIN := $(FISICS_DIR)/fisics
DESKTOP_APP_DIR ?= $(HOME)/Desktop/$(PACKAGE_APP_NAME)
PACKAGE_ADHOC_SIGN_IDENTITY ?= -
RELEASE_VERSION_FILE ?= VERSION
RELEASE_VERSION ?= $(strip $(shell cat "$(RELEASE_VERSION_FILE)" 2>/dev/null))
ifeq ($(RELEASE_VERSION),)
RELEASE_VERSION := 0.1.0
endif
RELEASE_CHANNEL ?= stable
RELEASE_PRODUCT_NAME := codeC
RELEASE_PROGRAM_KEY := ide
RELEASE_BUNDLE_ID := com.cosm.codec
RELEASE_ARTIFACT_BASENAME := $(RELEASE_PRODUCT_NAME)-$(RELEASE_VERSION)-$(RELEASE_PLATFORM)-$(RELEASE_ARCH)-$(RELEASE_CHANNEL)
RELEASE_DIR := build/release
RELEASE_APP_ZIP := $(RELEASE_DIR)/$(RELEASE_ARTIFACT_BASENAME).zip
RELEASE_MANIFEST := $(RELEASE_DIR)/$(RELEASE_ARTIFACT_BASENAME).manifest.txt
RELEASE_CODESIGN_IDENTITY ?= $(if $(strip $(APPLE_SIGN_IDENTITY)),$(APPLE_SIGN_IDENTITY),$(PACKAGE_ADHOC_SIGN_IDENTITY))
APPLE_SIGN_IDENTITY ?=
APPLE_NOTARY_PROFILE ?=
APPLE_TEAM_ID ?=
STAPLE_MAX_ATTEMPTS ?= 6
STAPLE_RETRY_DELAY_SEC ?= 15
