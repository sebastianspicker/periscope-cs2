if(LR_BUILD_TESTS)
  enable_testing()
  function(lr_test target)
    lr_exe(${target} tests/${target}.cpp ${ARGN})
    add_test(NAME ${target} COMMAND ${target})
  endfunction()

  if(TARGET ac_real)
    lr_exe(ac_scaffold_smoke tests/smoke_test.cpp ac_lab ac_server ac_depth ac_t0_red ac_t0_blue ac_t1_red ac_t1_blue ac_t2_red ac_t2_blue ac_t3_red ac_t3_blue ac_t4_red ac_t4_blue ac_real)
  else()
    lr_exe(ac_scaffold_smoke tests/smoke_test.cpp ac_lab ac_server ac_depth ac_t0_red ac_t0_blue ac_t1_red ac_t1_blue ac_t2_red ac_t2_blue ac_t3_red ac_t3_blue ac_t4_red ac_t4_blue)
  endif()
  add_test(NAME scaffold_smoke COMMAND ac_scaffold_smoke)
  lr_test(pipeline_unit_test ac_pipeline ac_sim ac_common)
  lr_test(sim_unit_test ac_sim ac_common)
  if(TARGET ac_real)
    lr_test(real_backend_test ac_real ac_sim ac_common)
  endif()
  if(TARGET ac_strategies)
    lr_test(architecture_contract_test ac_strategies)
  endif()
endif()

set(_PE_TARGETS radar_t0 radar_t1 radar_t2 radar_t3 radar_t4
  live_radar desktop_dup_overlay gui_demo full_prototype tier_comparison
  radar_multi_tier fps_demo evasion_lab features_lab ops_lab structural_lab
  cs2_radar_t0 cs2_radar_t1 cs2_radar_t2 cs2_radar_t3 cs2_radar_t4
  uefi_fw host_sim strategy_lab)
foreach(target ${_PE_TARGETS})
  if(TARGET ${target} AND WIN32)
    get_target_property(_type ${target} TYPE)
    if(_type STREQUAL "EXECUTABLE")
      add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -Dinput="$<TARGET_FILE:${target}>"
                -P "${RADAR_SOURCE_DIR}/cmake/strip_rich_header.cmake"
        COMMENT "Stripping Rich Header and timestamp from ${target}..."
        VERBATIM)
    endif()
  endif()
endforeach()
