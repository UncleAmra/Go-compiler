CC ?= gcc
CXX ?= g++
CFLAGS ?= -std=c11 -Wall -Wextra -pedantic -Wno-unused-parameter -Iinclude -O2
CXXFLAGS ?= -std=c++17 -Wall -Wextra -pedantic -Wno-unused-parameter -Iinclude -O2
BUILD_DIR ?= build

ifeq ($(OS),Windows_NT)
    MKDIR = if not exist $(BUILD_DIR) mkdir $(BUILD_DIR)
    RM = -rmdir /s /q $(BUILD_DIR) 2>nul || exit 0
else
    MKDIR = mkdir -p $(BUILD_DIR)
    RM = rm -rf $(BUILD_DIR)
endif

C_SRCS = src/common.c \
         src/token.c \
         src/lexer.c \
         src/ast.c \
         src/parser.c \
         src/typecheck.c

CXX_SRCS = src/tac.cpp \
           src/cfg.cpp \
           src/ssa.cpp \
           src/opt.cpp \
           src/codegen_ssa.cpp

C_OBJS = $(C_SRCS:src/%.c=$(BUILD_DIR)/%.o)
CXX_OBJS = $(CXX_SRCS:src/%.cpp=$(BUILD_DIR)/%.o)
ALL_OBJS = $(C_OBJS) $(CXX_OBJS)

TARGET = $(BUILD_DIR)/minigo.exe
TEST_LEXER = $(BUILD_DIR)/test_lexer.exe

all: $(TARGET) $(TEST_LEXER)

$(BUILD_DIR):
	$(MKDIR)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: src/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(ALL_OBJS) $(BUILD_DIR)/main.o
	$(CXX) $(CXXFLAGS) $^ -o $@

$(BUILD_DIR)/main.o: src/main.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TEST_LEXER): $(C_OBJS) tests/test_lexer.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@

test: $(TEST_LEXER)
	./$(TEST_LEXER)

clean:
	$(RM)

.PHONY: all test clean
