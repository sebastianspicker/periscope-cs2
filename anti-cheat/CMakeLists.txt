cmake_minimum_required(VERSION 3.20)
project(ac_lab LANGUAGES CXX)

# Anti-cheat lab (MIT): the simulation world, the tiered strategy curriculum
# and the lab applications. This tree owns the world; the dependency direction
# is strictly one-way (radar -> anti-cheat): radar consumes this tree's
# library cmake via LR_ANTI_CHEAT_DIR, and this tree never includes radar
# CMake. The single exception is the test-only LR_RADAR_DIR seam below.

# Modern Apple and Windows linkers do not need repeated static archives. Keep
# the explicit target graph readable while letting capable CMake versions emit
# a deterministic, de-duplicated link line.
if(POLICY CMP0156)
  cmake_policy(SET CMP0156 NEW)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(AC_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(AC_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")

# Test-only radar seam: stack_posture_test quote-includes the posture policy
# header from radar's adapters tree ("real/cs2/posture.hpp"). Nothing else in
# this project may reference the radar tree. The lab still configures and
# builds when the sibling tree is absent; only that host test is skipped.
set(LR_RADAR_DIR "../radar" CACHE PATH
  "Path to the radar product tree (test-only include seam for stack_posture_test)")
get_filename_component(LR_RADAR_DIR "${LR_RADAR_DIR}" ABSOLUTE
  BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
if(EXISTS "${LR_RADAR_DIR}/adapters/real/cs2/posture.hpp")
  set(AC_HAVE_RADAR TRUE)
else()
  set(AC_HAVE_RADAR FALSE)
  message(STATUS
    "Radar tree not found at ${LR_RADAR_DIR}; stack_posture_test is skipped")
endif()

# Shared helpers and lab options; the lab's own include order.
include("${CMAKE_CURRENT_LIST_DIR}/cmake/LrHelpers.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/Options.cmake")

# Lab libraries (domain, simulation, scenarios) and lab applications. The
# DomainTargets radar-side block stays skipped here (RADAR_SOURCE_DIR is
# deliberately undefined in this project); RealTargets is not included — the
# lab has no real backends.
include("${CMAKE_CURRENT_LIST_DIR}/cmake/DomainTargets.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/SimulationTargets.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/ScenarioTargets.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/AppTargets.cmake")

enable_testing()
add_subdirectory(tests)
