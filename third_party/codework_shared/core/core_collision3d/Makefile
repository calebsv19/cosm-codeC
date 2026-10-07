CC ?= cc
AR ?= ar
CFLAGS ?= -std=c11 -Wall -Wextra -Werror -O2 -ffp-contract=off
LDLIBS ?= -lm
BUILD_DIR ?= build
LIB = $(BUILD_DIR)/libcore_collision3d.a
OBJ = $(BUILD_DIR)/core_collision3d.o
TEST = $(BUILD_DIR)/core_collision3d_test
.PHONY: all test
all: $(LIB)
$(BUILD_DIR):
	mkdir -p "$@"
$(OBJ): src/core_collision3d.c include/core_collision3d.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@
$(LIB): $(OBJ)
	$(AR) rcs $@ $^
$(TEST): tests/core_collision3d_test.c $(LIB) include/core_collision3d.h
	$(CC) $(CFLAGS) -Iinclude $< $(LIB) $(LDLIBS) -o $@
test: $(TEST)
	$(TEST)
