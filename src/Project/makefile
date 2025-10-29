# === Project-local Makefile ===

# Output directory and final build path
OUTPUT_DIR ?= ./BuildOutputs
OUTPUT := $(OUTPUT_DIR)/last_build

# Find all .c files in current directory (flat only)
SRCS := $(wildcard *.c)
OBJS := $(SRCS:.c=.o)

# Compiler settings
CC := cc
CFLAGS := -Wall -Wextra -std=c99 `sdl2-config --cflags`
LDFLAGS := `sdl2-config --libs`

# Default target
all: $(OUTPUT)

$(OUTPUT): $(OBJS)
	@echo "[Makefile] Linking -> $(OUTPUT)"
	@mkdir -p $(OUTPUT_DIR)
	$(CC) $(OBJS) -o $(OUTPUT) $(LDFLAGS)

%.o: %.c
	@echo "[Makefile] Compiling $<"
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@echo "[Makefile] Cleaning..."
	rm -f *.o $(OUTPUT)

