# ===== CONFIGURATION =====
CC = cc
CFLAGS = -g -Wall -I./src -std=c99 -MMD -MP
LDFLAGS = -lSDL2 -lSDL2_ttf -ljson-c


SRC_DIR := src
BUILD_DIR := build
EXCLUDE_DIR := $(SRC_DIR)/Project

# Source, object, and dependency file discovery
SRC_FILES := $(shell find $(SRC_DIR) -type f -name '*.c' ! -path "$(EXCLUDE_DIR)/*")
OBJ_FILES := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC_FILES))
DEP_FILES := $(OBJ_FILES:.o=.d)

OUT = ide

# ===== RULES =====

all: $(OUT)

# Link all object files into the final binary
$(OUT): $(OBJ_FILES)
	@echo "Linking executable..."
	@$(CC) -o $@ $^ $(LDFLAGS) || (echo "Linking failed!" && exit 1)

# Compile each .c file into a .o file + generate .d file
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "Compiling $<"
	@$(CC) $(CFLAGS) -c $< -o $@ || (echo "Compile failed for $<" && exit 1)

# Run the built binary
run: $(OUT)
	@./$(OUT)

# Clean all build artifacts
clean:
	@rm -rf $(BUILD_DIR) $(OUT)
	@echo "Cleaned up build artifacts."

# Include dependency files if they exist
-include $(DEP_FILES)
