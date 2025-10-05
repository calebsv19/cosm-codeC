# ===== CONFIGURATION =====
CC = cc

# Detect Homebrew prefix (works on Intel and Apple Silicon)
BREW_PREFIX := $(shell brew --prefix 2>/dev/null)

# Include dir should be the PARENT of SDL2, not the SDL2 folder itself
INC_DIRS := -I./src -I$(BREW_PREFIX)/include
LIB_DIRS := -L$(BREW_PREFIX)/lib

CFLAGS  = -g -Wall -std=c99 -MMD -MP $(INC_DIRS)
LDFLAGS = $(LIB_DIRS) -lSDL2 -lSDL2_ttf -lSDL2_image -lSDL2_mixer -ljson-c

SRC_DIR := src
BUILD_DIR := build
EXCLUDE_DIR := $(SRC_DIR)/Project

SRC_FILES := $(shell find $(SRC_DIR) -type f -name '*.c' ! -path "$(EXCLUDE_DIR)/*")
OBJ_FILES := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC_FILES))
DEP_FILES := $(OBJ_FILES:.o=.d)

OUT = ide

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

-include $(DEP_FILES)
