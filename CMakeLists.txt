cmake_minimum_required(VERSION 3.20)
project(RevaSM LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(REVASM_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

add_library(revasm_core
    src/core/semantics.cpp
    src/core/verifier.cpp
    src/core/interpreter.cpp
)

target_include_directories(revasm_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_executable(revasm_semantic_tests
    tests/semantic/tests.cpp
)

target_link_libraries(revasm_semantic_tests PRIVATE revasm_core)

foreach(target revasm_core revasm_semantic_tests)
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Werror>
    )

    if(REVASM_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
        )
    endif()
endforeach()

if(REVASM_ENABLE_SANITIZERS AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_link_options(revasm_semantic_tests PRIVATE
        -fsanitize=address,undefined
    )
endif()

enable_testing()
add_test(NAME semantic COMMAND revasm_semantic_tests)
