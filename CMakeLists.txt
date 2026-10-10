cmake_minimum_required(VERSION 3.16)
project(clair C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_executable(clair
    src/main.c
    src/lexer.c
    src/ast.c
    src/parser.c
    src/semantic.c
    src/codegen_c.c
)
target_compile_options(clair PRIVATE -Wall -Wextra -Wpedantic)
