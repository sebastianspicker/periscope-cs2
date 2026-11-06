cmake_minimum_required(VERSION 3.20)
project(periscope_radar LANGUAGES C CXX)

# Two-tree layout: this radar product tree consumes the anti-cheat lab subtree
# (../anti-cheat) for its simulation world, lab components and strategy
# libraries. The dependency direction is strictly one-way (radar ->
# anti-cheat): the lab never includes radar cmake or sources. The seam below is
# the only path wiring; all library cmake for the lab lives in
# ${LR_ANTI_CHEAT_DIR}/cmake.
set(LR_ANTI_CHEAT_DIR "../anti-cheat" CACHE PATH "Path to the anti-cheat lab subtree")
get_filename_component(LR_ANTI_CHEAT_DIR "${LR_ANTI_CHEAT_DIR}" ABSOLUTE
  BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
if(NOT EXISTS "${LR_ANTI_CHEAT_DIR}/cmake/LrHelpers.cmake")
  message(FATAL_ERROR "LR_ANTI_CHEAT_DIR does not point at an anti-cheat checkout: ${LR_ANTI_CHEAT_DIR}")
endif()
# Lab sources live in the anti-cheat tree; the anti-cheat cmake files resolve
# lab-side paths against AC_SOURCE_DIR (defined by the includer, mirroring the
# stub anti-cheat/CMakeLists.txt) and radar-side paths against RADAR_SOURCE_DIR.
set(AC_SOURCE_DIR "${LR_ANTI_CHEAT_DIR}")

# Modern Apple and Windows linkers do not need repeated static archives. Keep
# the explicit target graph readable while letting capable CMake versions emit
# a deterministic, de-duplicated link line.
if(POLICY CMP0156)
  cmake_policy(SET CMP0156 NEW)
endif()

# Radar is intentionally self-contained: no target includes or links Vision.
if(MSVC)
  enable_language(ASM_MASM)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(RADAR_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(RADAR_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

# Shared lr_* helpers and lab options come from the anti-cheat tree; radar
# options (real flags, hardening) stay local. Order: LrHelpers first (it owns
# lr_build_options), then lab options, then radar options.
include("${LR_ANTI_CHEAT_DIR}/cmake/LrHelpers.cmake")
include("${LR_ANTI_CHEAT_DIR}/cmake/Options.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/Options.cmake")
include("${LR_ANTI_CHEAT_DIR}/cmake/DomainTargets.cmake")
include("${LR_ANTI_CHEAT_DIR}/cmake/SimulationTargets.cmake")
include("${LR_ANTI_CHEAT_DIR}/cmake/ScenarioTargets.cmake")
# Lab applications (S5): the lab's AppTargets registers the sim-only demos,
# ac_pipeline and strategy_lab in this configure too, so the radar-sim verify
# lane can build/run them from radar's build dir. It must be included before
# radar's own AppTargets: cs2_radar_t0's sim fallback links the lab's
# ac_pipeline.
include("${LR_ANTI_CHEAT_DIR}/cmake/AppTargets.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/RealTargets.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/AppTargets.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/TestTargets.cmake")
enable_testing()
add_subdirectory(tests)
