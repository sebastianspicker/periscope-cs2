# CMakeLists.txt — T4 PCIe DMA firmware example
#
# Builds:
#   host_sim  — full T4 PCIe DMA CS2 entity read simulation
#   dma_tests — unit tests for the page walker / entity reader / FPGA model
#
# Usage:
#   cmake -S . -B build
#   cmake --build build
#   ./build/bin/host_sim
#   ctest --test-dir build --output-on-failure
#
# Options:
#   BUILD_SIM    Build host_sim (ON by default)
#   BUILD_TESTS  Build dma_tests + CTest (ON by default)
#   ENABLE_SANITIZERS  Address/UB sanitizers for GCC/Clang (OFF)

cmake_minimum_required(VERSION 3.20)
project(t4-pcie-dma LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_SIM "Build the PCIe DMA simulator" ON)
option(BUILD_TESTS "Build the DMA unit tests" ON)
option(ENABLE_SANITIZERS "Enable ASan/UBSan (GCC/Clang)" OFF)

# ── Compiler warnings / hardening ─────────────────────────────────────
if(MSVC)
    add_compile_options(/W4 /permissive- /EHsc)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

if(ENABLE_SANITIZERS AND NOT MSVC)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

# ── Core library (shared sources) ─────────────────────────────────────
add_library(dma_core STATIC
    page_table_walker.cpp
    cs2_dma_reader.cpp
)
target_include_directories(dma_core PUBLIC .)
target_compile_definitions(dma_core PRIVATE _CRT_SECURE_NO_WARNINGS)

# ── host_sim ──────────────────────────────────────────────────────────
if(BUILD_SIM)
    add_executable(host_sim
        host_sim.cpp
        host_sim_memory.cpp
        host_sim_fpga.cpp
    )
    target_link_libraries(host_sim PRIVATE dma_core)
    set_target_properties(host_sim PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endif()

# ── dma_tests ─────────────────────────────────────────────────────────
if(BUILD_TESTS)
    enable_testing()
    add_executable(dma_tests tests/dma_tests.cpp)
    target_link_libraries(dma_tests PRIVATE dma_core)
    set_target_properties(dma_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    add_test(NAME dma_tests COMMAND dma_tests)
endif()
