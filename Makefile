CC ?= gcc
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic -Wno-unused-parameter -Iinclude -O2
BUILD_DIR ?= build

SRCS = src/common.c \
       src/token.c \
       src/lexer.c \
       src/ast.c \
       src/parser.c \
       src/typecheck.c \
       src/codegen_x86.c

OBJS = $(SRCS:src/%.c=$(BUILD_DIR)/%.o)

TARGET = $(BUILD_DIR)/minigo.exe
TEST_LEXER = $(BUILD_DIR)/test_lexer.exe

all: $(TARGET) $(TEST_LEXER)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS) $(BUILD_DIR)/main.o
	$(CC) $(CFLAGS) $^ -o $@

$(TEST_LEXER): $(OBJS) tests/test_lexer.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@

test: $(TEST_LEXER)
	./$(TEST_LEXER)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all test clean
