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
