# Shared lr_* CMake helpers, owned by the anti-cheat lab tree.
#
# Include order matters: whoever composes a tree must include this file FIRST
# (it creates the lr_build_options interface), then anti-cheat's Options.cmake,
# then radar's cmake/Options.cmake.
#
# The helpers are tree-agnostic: they resolve paths against
# CMAKE_CURRENT_SOURCE_DIR of the caller (radar root or anti-cheat root), never
# against a hardcoded tree root.

# Shared compile/link options for every lr_* target. Created idempotently so
# both trees can include this file without conflicting.
if(NOT TARGET lr_build_options)
  add_library(lr_build_options INTERFACE)
  # Portable warning baseline for every lr_* target. Not -Werror: warning
  # policy should not make a lab build fail on a new compiler release.
  if(MSVC)
    target_compile_options(lr_build_options INTERFACE /W4)
  else()
    target_compile_options(lr_build_options INTERFACE -Wall -Wextra)
  endif()
endif()

function(lr_library target)
  if(NOT ARGN)
    message(FATAL_ERROR "${target} has no sources in the Radar layout")
  endif()
  add_library(${target} ${ARGN})
  target_link_libraries(${target} PUBLIC lr_build_options)
endfunction()

function(lr_exe target source)
  # Tree-agnostic source resolution: absolute paths are used as-is; relative
  # paths resolve against the caller's tree root.
  set(_lr_exe_source "${source}")
  if(NOT IS_ABSOLUTE "${_lr_exe_source}")
    set(_lr_exe_source "${CMAKE_CURRENT_SOURCE_DIR}/${_lr_exe_source}")
  endif()
  if(NOT EXISTS "${_lr_exe_source}")
    message(FATAL_ERROR "${target} source is missing: ${source}")
  endif()
  add_executable(${target} "${_lr_exe_source}")
  target_link_libraries(${target} PRIVATE lr_build_options ${ARGN})
  # Applications are composition roots; reusable targets never receive this path.
  # Only the radar tree has an adapters/ directory today.
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/adapters")
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/adapters")
  endif()
endfunction()

# Platform classification. Generic, needed by every tree: the lab simulates the
# same platform vocabulary the real adapters observe. REAL_PLATFORM_LIBS is a
# normal variable consumed by radar's RealTargets.cmake when real backends are
# enabled.
if(WIN32)
  target_compile_definitions(lr_build_options INTERFACE LR_PLATFORM_WINDOWS=1 LR_PLATFORM_LINUX=0)
  set(REAL_PLATFORM_LIBS "")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_compile_definitions(lr_build_options INTERFACE LR_PLATFORM_WINDOWS=0 LR_PLATFORM_LINUX=1)
  set(REAL_PLATFORM_LIBS pthread dl rt)
else()
  target_compile_definitions(lr_build_options INTERFACE LR_PLATFORM_WINDOWS=0 LR_PLATFORM_LINUX=0)
endif()
