if(LR_BUILD_STRATEGY_LAB)
  include("${CMAKE_CURRENT_LIST_DIR}/ScenarioSources.cmake")
  set(AC_STRATEGIES ${AC_STRATEGY_SCENARIO_SOURCES}
    "${RADAR_SOURCE_DIR}/scenarios/strategies/framework/registry.cpp"
    "${RADAR_SOURCE_DIR}/scenarios/strategies/framework/registry_run.cpp"
    "${RADAR_SOURCE_DIR}/scenarios/strategies/framework/registry_dual.cpp")
  lr_library(ac_strategies ${AC_STRATEGIES})
target_include_directories(ac_strategies PUBLIC "${RADAR_SOURCE_DIR}/scenarios")
  target_link_libraries(ac_strategies PUBLIC
    ac_strategy_core ac_common ac_sim ac_blue ac_server ac_depth ac_fps ac_cs2
    ac_cs2_signatures ac_lab ac_lab_scanner ac_t0_red ac_t0_blue ac_t1_red
    ac_t1_blue ac_t2_red ac_t2_blue ac_t3_red ac_t3_blue ac_t4_red ac_t4_blue)
endif()
