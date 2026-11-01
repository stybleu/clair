cmake_minimum_required(VERSION 3.16)
project(clair C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
add_executable(clair
    src/main.c
    src/diagnostics.c
    src/dce.c
    src/ad.c
    src/ad_reverse.c
    src/lexer.c
    src/ast.c
    src/parser.c
    src/semantic.c
    src/modules.c
    src/units.c
    src/codegen_c.c
    src/web.c
)
target_compile_options(clair PRIVATE -Wall -Wextra -Wpedantic)

# V0.42 : the bigint/bigfloat runtime is copied into the generated C,
# so `clair` works from any folder (no src/ needed next to the program).
set(CLAIR_BIG_PARTS
    src/clair_bigint.h src/clair_bigint.c src/clair_bigfloat.h src/clair_bigfloat.c
    src/clair_bigmath.h src/clair_bigmath.c src/clair_big_numbers_rt.h src/clair_big_numbers_rt.c
    src/clair_certfloat.h src/clair_certfloat.c src/clair_certfloat_rt.c)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/clair_big_runtime.h
    COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR}
            -DOUT=${CMAKE_CURRENT_BINARY_DIR}/clair_big_runtime.h
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_runtime.cmake
    DEPENDS ${CLAIR_BIG_PARTS} tools/embed_runtime.cmake
    COMMENT "Embedding the big-number runtime")
target_sources(clair PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/clair_big_runtime.h)

# V0.53: the measure runtime (value ± uncertainty), copied into the programs that use measures
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/clair_measure_runtime.h
    COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR}
            "-DIN=clair_measure.h\;clair_measure_rt.c" -DNAME=clair_measure_runtime_text
            -DOUT=${CMAKE_CURRENT_BINARY_DIR}/clair_measure_runtime.h
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_file.cmake
    DEPENDS src/clair_measure.h src/clair_measure_rt.c tools/embed_file.cmake
    COMMENT "Embedding the measure runtime")
target_sources(clair PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/clair_measure_runtime.h)

# V0.55: the module web: its Clair part (read by the compiler as the module web),
# and its C part, copied into the programs that import web
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/clair_web_module.h
    COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR} -DRAW=1
            "-DIN=clair_web.cl" -DNAME=clair_web_module_text
            -DOUT=${CMAKE_CURRENT_BINARY_DIR}/clair_web_module.h
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_file.cmake
    DEPENDS src/clair_web.cl tools/embed_file.cmake
    COMMENT "Embedding the module web")
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/clair_web_runtime.h
    COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR}
            "-DIN=clair_web_rt.c" -DNAME=clair_web_runtime_text
            -DOUT=${CMAKE_CURRENT_BINARY_DIR}/clair_web_runtime.h
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_file.cmake
    COMMAND ${CMAKE_COMMAND} -DSRC=${CMAKE_CURRENT_SOURCE_DIR}
            "-DIN=clair_web_prelude.c" -DNAME=clair_web_prelude_text
            -DOUT=${CMAKE_CURRENT_BINARY_DIR}/clair_web_prelude.h
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tools/embed_file.cmake
    BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/clair_web_prelude.h
    DEPENDS src/clair_web_rt.c src/clair_web_prelude.c tools/embed_file.cmake
    COMMENT "Embedding the web server")
target_sources(clair PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/clair_web_module.h ${CMAKE_CURRENT_BINARY_DIR}/clair_web_runtime.h)
target_include_directories(clair PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
