# Lab application targets (S5): the sim-only demos, the ac_pipeline
# application service, and the strategy_lab catalog runner. Everything here
# links only ac_* lab libraries — no real adapter target may appear in this
# file. Lab paths resolve against AC_SOURCE_DIR (defined by the includer: the
# anti-cheat root CMakeLists or radar's).
#
# This file is included by BOTH trees: radar's root CMakeLists includes it so
# the radar-sim verify lane can build strategy_lab (and the lab demos) from
# radar's build dir. Registrations must therefore be safe to run in a radar
# configure too; strategy_lab's dual registration is resolved by the
# NOT TARGET guard below combined with the same guard in radar's AppTargets.

# The pipeline is an application service: it composes simulation services but
# never links a real adapter. Real executable composition happens in radar's
# AppTargets; the cs2_radar_t0 sim fallback links this target.
lr_library(ac_pipeline
  "${AC_SOURCE_DIR}/src/application/radar_pipeline.cpp")
target_link_libraries(ac_pipeline PUBLIC ac_sim ac_common)
target_include_directories(ac_pipeline PUBLIC "${AC_SOURCE_DIR}/src/application")

lr_exe(fps_demo "${AC_SOURCE_DIR}/apps/demos/fps_demo.cpp" ac_fps ac_server)
target_include_directories(fps_demo PRIVATE "${AC_SOURCE_DIR}/apps")

if(LR_BUILD_PROTOS)
  foreach(tier 0 1 2 3 4)
    lr_exe(proto_t${tier}_red "${AC_SOURCE_DIR}/apps/demos/proto_t${tier}_red.cpp" ac_t${tier}_red)
    lr_exe(proto_t${tier}_blue "${AC_SOURCE_DIR}/apps/demos/proto_t${tier}_blue.cpp" ac_t${tier}_blue ac_t${tier}_red)
  endforeach()
  target_link_libraries(proto_t1_blue PRIVATE ac_t0_blue)
  target_link_libraries(proto_t2_blue PRIVATE ac_t0_blue)
endif()
if(LR_BUILD_DUELS)
  foreach(tier 0 1 2 3 4)
    lr_exe(duel_t${tier} "${AC_SOURCE_DIR}/apps/demos/duel_t${tier}.cpp" ac_t${tier}_red ac_t${tier}_blue)
  endforeach()
  target_link_libraries(duel_t1 PRIVATE ac_t0_red ac_t0_blue)
  target_link_libraries(duel_t2 PRIVATE ac_t0_red ac_t0_blue)
  target_link_libraries(duel_t3 PRIVATE ac_t2_red ac_t0_red ac_server)
  target_link_libraries(duel_t4 PRIVATE ac_server)
endif()
if(TARGET ac_strategies)
  lr_exe(strategy_lab "${AC_SOURCE_DIR}/scenarios/strategies/framework/main.cpp" ac_strategies)
  if(LR_BUILD_DUELS)
    lr_exe(evasion_lab "${AC_SOURCE_DIR}/apps/demos/evasion_lab.cpp" ac_strategies)
    lr_exe(features_lab "${AC_SOURCE_DIR}/apps/demos/features_lab.cpp" ac_strategies)
    lr_exe(ops_lab "${AC_SOURCE_DIR}/apps/demos/ops_lab.cpp" ac_strategies)
    lr_exe(structural_lab "${AC_SOURCE_DIR}/apps/demos/structural_lab.cpp" ac_strategies)
  endif()
endif()

# PE hardening mirrors radar/cmake/TestTargets.cmake: the Rich Header strip
# only fires on WIN32 and skips targets that do not exist in this configure.
set(_AC_PE_TARGETS
  fps_demo
  proto_t0_red proto_t0_blue proto_t1_red proto_t1_blue
  proto_t2_red proto_t2_blue proto_t3_red proto_t3_blue
  proto_t4_red proto_t4_blue
  duel_t0 duel_t1 duel_t2 duel_t3 duel_t4
  evasion_lab features_lab ops_lab structural_lab strategy_lab)
foreach(target ${_AC_PE_TARGETS})
  if(TARGET ${target} AND WIN32)
    get_target_property(_type ${target} TYPE)
    if(_type STREQUAL "EXECUTABLE")
      add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -Dinput="$<TARGET_FILE:${target}>"
                -P "${CMAKE_CURRENT_LIST_DIR}/strip_rich_header.cmake"
        COMMENT "Stripping Rich Header and timestamp from ${target}..."
        VERBATIM)
    endif()
  endif()
endforeach()
