cmake_minimum_required(VERSION 3.15)
project(minigo C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -pedantic -Wno-unused-parameter)
endif()

include_directories(include)

set(MINIGO_C_SOURCES
    src/common.c
    src/token.c
    src/lexer.c
    src/ast.c
    src/parser.c
    src/typecheck.c
)

set(MINIGO_CXX_SOURCES
    src/tac.cpp
    src/cfg.cpp
    src/ssa.cpp
    src/opt.cpp
    src/codegen_ssa.cpp
)

add_library(minigo_lib STATIC ${MINIGO_C_SOURCES} ${MINIGO_CXX_SOURCES})

add_executable(minigo src/main.cpp)
target_link_libraries(minigo PRIVATE minigo_lib)

# Tests
add_executable(test_lexer tests/test_lexer.c)
target_link_libraries(test_lexer PRIVATE minigo_lib)
