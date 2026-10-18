cmake_minimum_required(VERSION 3.15)
project(minigo C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -pedantic -Wno-unused-parameter)
endif()

include_directories(include)

set(MINIGO_LIB_SOURCES
    src/common.c
    src/token.c
    src/lexer.c
    src/ast.c
    src/parser.c
    src/typecheck.c
    src/codegen_x86.c
)

add_library(minigo_lib STATIC ${MINIGO_LIB_SOURCES})

add_executable(minigo src/main.c)
target_link_libraries(minigo PRIVATE minigo_lib)

# Tests
add_executable(test_lexer tests/test_lexer.c)
target_link_libraries(test_lexer PRIVATE minigo_lib)
