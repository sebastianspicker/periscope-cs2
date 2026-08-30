# Linux real-adapter targets for Periscope Radar.
#
# Targets:
#   ac_linux           — Full userspace stack
#   ac_linux_mem|proc|pcie — INTERFACE aliases (compat)
#   aclab_client       — CLI for /dev/aclab (Linux only)
#   linux_stack_test   — Unit tests (host + Linux)
#
# Kernel module (aclab_module.ko) is built by the Makefile via Kbuild.

cmake_minimum_required(VERSION 3.20)
# Standalone configure: `cmake -S radar/adapters/real/linux -B build`
# When included via add_subdirectory from the root project, skip re-project().
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(ac_linux_stack LANGUAGES CXX)
  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

set(AC_LINUX_SOURCES
    memory.cpp
    memory_helpers.cpp
    process.cpp
    pcie.cpp
    kmod_client.cpp
    anti_debug.cpp
    hook_detect.cpp
    module_enum.cpp
    page_walk.cpp
    bpf_probe.cpp
    netlink_audit.cpp
)

# radar/adapters so #include "real/..." resolves.
get_filename_component(AC_LIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

add_library(ac_linux STATIC ${AC_LINUX_SOURCES})
target_include_directories(ac_linux PUBLIC
    "${AC_LIB_ROOT}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(ac_linux PUBLIC LR_PLATFORM_LINUX=1 LR_PLATFORM_WINDOWS=0)
    find_package(Threads REQUIRED)
    target_link_libraries(ac_linux PUBLIC Threads::Threads)
else()
    target_compile_definitions(ac_linux PUBLIC LR_PLATFORM_LINUX=0)
    if(WIN32)
        target_compile_definitions(ac_linux PUBLIC LR_PLATFORM_WINDOWS=1 _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_definitions(ac_linux PUBLIC LR_PLATFORM_WINDOWS=0)
    endif()
endif()

# Preserve the established standalone target names for external build scripts.
add_library(ac_linux_mem INTERFACE)
target_link_libraries(ac_linux_mem INTERFACE ac_linux)
add_library(ac_linux_proc INTERFACE)
target_link_libraries(ac_linux_proc INTERFACE ac_linux)
add_library(ac_linux_pcie INTERFACE)
target_link_libraries(ac_linux_pcie INTERFACE ac_linux)

enable_testing()
add_executable(linux_stack_test tests/linux_stack_test.cpp)
target_link_libraries(linux_stack_test PRIVATE ac_linux)
add_test(NAME linux_stack_test COMMAND linux_stack_test)
set_tests_properties(linux_stack_test PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_executable(aclab_client aclab_client.cpp)
    target_link_libraries(aclab_client PRIVATE ac_linux)

    add_custom_target(aclab_module
        COMMAND make -C "${CMAKE_CURRENT_SOURCE_DIR}" all
        COMMENT "Building aclab_module.ko via Makefile/Kbuild..."
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

