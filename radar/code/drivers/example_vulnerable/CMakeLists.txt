# CMakeLists.txt — Build vuln_driver.sys, user_test.exe, gdrv_host_tests.exe
#
# Usermode (always works with MSVC / Windows SDK):
#   cmake -S . -B build -DBUILD_DRIVER=OFF
#   cmake --build build --config Release
#   build\Release\user_test.exe --info
#   build\Release\gdrv_host_tests.exe
#
# Kernel driver (requires WDK km headers + libs):
#   cmake -S . -B build -DBUILD_DRIVER=ON -DWDK_ROOT="C:\Program Files (x86)\Windows Kits\10"
#   cmake --build build --config Release

cmake_minimum_required(VERSION 3.20)
project(gdrv-byovd LANGUAGES C CXX)

option(BUILD_DRIVER "Build the kernel driver (requires WDK km)" OFF)
option(BUILD_TEST "Build the usermode test tool" ON)
option(BUILD_HOST_TESTS "Build host ABI unit tests" ON)

# ── Shared include for ABI headers ───────────────────────────────────
set(GDRV_PUBLIC_INC "${CMAKE_CURRENT_SOURCE_DIR}")

# ── Usermode test tool ──────────────────────────────────────────────
if(BUILD_TEST)
    add_executable(user_test user_test.cpp)
    target_include_directories(user_test PRIVATE ${GDRV_PUBLIC_INC})
    target_compile_features(user_test PRIVATE cxx_std_20)
    target_compile_definitions(user_test PRIVATE _CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN)
    target_link_libraries(user_test PRIVATE advapi32 psapi)
    # Console subsystem (not WIN32) so --info/--list print to stdout
    if(MSVC)
        target_link_options(user_test PRIVATE /SUBSYSTEM:CONSOLE)
    endif()
    set_target_properties(user_test PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/bin"
    )
endif()

# ── Host unit tests (no driver load) ────────────────────────────────
if(BUILD_HOST_TESTS)
    add_executable(gdrv_host_tests host_tests.cpp)
    target_include_directories(gdrv_host_tests PRIVATE ${GDRV_PUBLIC_INC})
    target_compile_features(gdrv_host_tests PRIVATE cxx_std_20)
    target_compile_definitions(gdrv_host_tests PRIVATE _CRT_SECURE_NO_WARNINGS)
    if(MSVC)
        target_link_options(gdrv_host_tests PRIVATE /SUBSYSTEM:CONSOLE)
    endif()
    set_target_properties(gdrv_host_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_BINARY_DIR}/bin"
    )
    enable_testing()
    add_test(NAME gdrv_host_tests COMMAND gdrv_host_tests)
endif()

# ── Kernel driver (optional WDK) ────────────────────────────────────
if(BUILD_DRIVER)
    if(NOT DEFINED WDK_ROOT OR WDK_ROOT STREQUAL "")
        set(WDK_ROOT "C:/Program Files (x86)/Windows Kits/10" CACHE PATH "WDK root")
    endif()

    # Discover newest km-capable kit version
    set(WDK_VER "")
    foreach(cand 10.0.26100.0 10.0.22621.0 10.0.22000.0 10.0.19041.0)
        if(EXISTS "${WDK_ROOT}/Include/${cand}/km/ntddk.h")
            set(WDK_VER "${cand}")
            break()
        endif()
    endforeach()

    if(WDK_VER STREQUAL "")
        # Fall back: scan Include dir
        file(GLOB _wdk_inc_vers LIST_DIRECTORIES true "${WDK_ROOT}/Include/*")
        list(SORT _wdk_inc_vers ORDER DESCENDING)
        foreach(d ${_wdk_inc_vers})
            if(EXISTS "${d}/km/ntddk.h")
                get_filename_component(WDK_VER "${d}" NAME)
                break()
            endif()
        endforeach()
    endif()

    if(WDK_VER STREQUAL "" OR NOT EXISTS "${WDK_ROOT}/Include/${WDK_VER}/km/ntddk.h")
        message(WARNING "WDK km headers not found under ${WDK_ROOT}. Driver target skipped.")
        message(WARNING "Install Windows Driver Kit, or configure with -DBUILD_DRIVER=OFF.")
    elseif(NOT EXISTS "${WDK_ROOT}/Lib/${WDK_VER}/km/x64/ntoskrnl.lib")
        message(WARNING "WDK km x64 libs not found (ntoskrnl.lib). Driver target skipped.")
    else()
        set(WDK_INC "${WDK_ROOT}/Include/${WDK_VER}")
        set(WDK_LIB "${WDK_ROOT}/Lib/${WDK_VER}/km/x64")

        add_library(vuln_driver_obj OBJECT
            vuln_driver.c
            vuln_driver_memory.c
            vuln_driver_notify.c
        )
        target_include_directories(vuln_driver_obj PRIVATE
            ${GDRV_PUBLIC_INC}
            "${WDK_INC}/km"
            "${WDK_INC}/km/crt"
            "${WDK_INC}/shared"
        )
        target_compile_definitions(vuln_driver_obj PRIVATE
            _AMD64_=1 AMD64=1
            NTDDI_VERSION=0x0A00000A
            _WIN32_WINNT=0x0A00
            DEPRECATE_DDK_FUNCTIONS=1
        )
        if(MSVC)
            target_compile_options(vuln_driver_obj PRIVATE
                /nologo /W3 /O2 /Gy /GR- /GS- /kernel
            )
        endif()

        add_custom_command(
            OUTPUT "${CMAKE_BINARY_DIR}/vuln_driver.sys"
            COMMAND link.exe /driver /subsystem:native /entry:DriverEntry
                    /nologo /incremental:no /opt:ref /opt:icf
                    /machine:x64
                    /out:"${CMAKE_BINARY_DIR}/vuln_driver.sys"
                    "${WDK_LIB}/ntoskrnl.lib"
                    "${WDK_LIB}/hal.lib"
                    "$<TARGET_OBJECTS:vuln_driver_obj>"
            DEPENDS vuln_driver_obj
            COMMENT "Linking kernel driver vuln_driver.sys (WDK ${WDK_VER})"
            VERBATIM
            COMMAND_EXPAND_LISTS
        )
        add_custom_target(driver ALL DEPENDS "${CMAKE_BINARY_DIR}/vuln_driver.sys")

        message(STATUS "WDK driver build enabled")
        message(STATUS "  WDK root: ${WDK_ROOT}")
        message(STATUS "  WDK ver:  ${WDK_VER}")
        message(STATUS "  Target:   vuln_driver.sys")
    endif()
endif()

message(STATUS "gdrv BYOVD project configured")
message(STATUS "  BUILD_DRIVER:     ${BUILD_DRIVER}")
message(STATUS "  BUILD_TEST:       ${BUILD_TEST}")
message(STATUS "  BUILD_HOST_TESTS: ${BUILD_HOST_TESTS}")
