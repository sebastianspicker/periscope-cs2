set(_app "${RADAR_SOURCE_DIR}/apps/demos")

# The pipeline is an application service: it composes simulation services but
# never links a real adapter. Real executable composition happens below.
lr_library(ac_pipeline
  "${RADAR_SOURCE_DIR}/src/application/radar_pipeline.cpp")
target_link_libraries(ac_pipeline PUBLIC ac_sim ac_common)
target_include_directories(ac_pipeline PUBLIC "${RADAR_SOURCE_DIR}/src/application")

lr_exe(fps_demo apps/demos/fps_demo.cpp ac_fps ac_server)
target_include_directories(fps_demo PRIVATE "${RADAR_SOURCE_DIR}/apps")
foreach(tier 0 1 2 3 4)
  lr_exe(radar_t${tier} apps/demos/radar_t${tier}.cpp ac_common ac_sim)
  target_include_directories(radar_t${tier} PRIVATE "${RADAR_SOURCE_DIR}/apps")
endforeach()

if(TARGET ac_real_rpm AND TARGET ac_real_syscall AND TARGET ac_real_gpu)
  # Real composition retains the former platform pipeline separately from the
  # inward simulation service. It is never linked by default targets.
  lr_library(ac_real_pipeline
    "${RADAR_SOURCE_DIR}/adapters/real/application/radar_pipeline.cpp"
    "${RADAR_SOURCE_DIR}/adapters/real/application/radar_pipeline_lifecycle.cpp"
    "${RADAR_SOURCE_DIR}/adapters/real/application/radar_pipeline_collect.cpp"
    "${RADAR_SOURCE_DIR}/adapters/real/application/radar_pipeline_frame.cpp")
  target_include_directories(ac_real_pipeline PUBLIC
    "${RADAR_SOURCE_DIR}/adapters/real/application")
  target_include_directories(ac_real_pipeline PRIVATE "${RADAR_SOURCE_DIR}/adapters")
  target_link_libraries(ac_real_pipeline PUBLIC
    ac_real_rpm ac_real_syscall ac_real_gpu ac_sim ac_common)
  if(TARGET ac_real_shellcode)
    target_link_libraries(ac_real_pipeline PUBLIC ac_real_shellcode)
  endif()

  add_executable(live_radar
    "${_app}/live_radar.cpp"
    "${_app}/live_radar_support.cpp"
    "${_app}/live_radar_loop.cpp")
  target_link_libraries(live_radar PRIVATE
    lr_build_options ac_real_rpm ac_real_syscall ac_real_gpu ac_sim ac_common)
  target_include_directories(live_radar PRIVATE "${RADAR_SOURCE_DIR}/apps")
  foreach(probe donor_worker entity_probe radar_diag scale_probe hud_probe icvar_probe)
    if(EXISTS "${_app}/${probe}.cpp")
      lr_exe(${probe} apps/demos/${probe}.cpp ac_real_rpm ac_common)
    endif()
  endforeach()
  lr_exe(desktop_dup_overlay apps/demos/desktop_dup_overlay.cpp ac_real_gpu)
  lr_exe(tier_comparison apps/demos/tier_comparison.cpp ac_real_rpm ac_real_syscall ac_common ac_sim)
  lr_exe(radar_multi_tier apps/demos/radar_multi_tier.cpp ac_real_rpm ac_real_syscall ac_common ac_sim)
  lr_exe(full_prototype apps/demos/full_prototype.cpp ac_real_rpm ac_real_syscall ac_real_gpu ac_common ac_sim)
  lr_exe(gui_demo apps/demos/gui_demo.cpp ac_real_gpu ac_common)
  if(TARGET ac_real_uefi)
    lr_exe(uefi_fw apps/demos/uefi_firmware/main.cpp ac_real_uefi ac_common)
  endif()
else()
  lr_exe(tier_comparison apps/demos/tier_comparison.cpp ac_common ac_sim)
endif()

if(LR_BUILD_PROTOS)
  foreach(tier 0 1 2 3 4)
    lr_exe(proto_t${tier}_red apps/demos/proto_t${tier}_red.cpp ac_t${tier}_red)
    lr_exe(proto_t${tier}_blue apps/demos/proto_t${tier}_blue.cpp ac_t${tier}_blue ac_t${tier}_red)
    add_executable(cs2_radar_t${tier}
      "${_app}/cs2_radar/t${tier}/cs2_radar.cpp"
      "${_app}/cs2_radar/t${tier}/main.cpp")
    target_link_libraries(cs2_radar_t${tier} PRIVATE lr_build_options)
    target_include_directories(cs2_radar_t${tier} PRIVATE
      "${RADAR_SOURCE_DIR}/apps"
      "${_app}/cs2_radar/t${tier}")
  endforeach()
  target_link_libraries(proto_t1_blue PRIVATE ac_t0_blue)
  target_link_libraries(proto_t2_blue PRIVATE ac_t0_blue)
  if(TARGET ac_real_pipeline)
    target_link_libraries(cs2_radar_t0 PRIVATE ac_real_pipeline ac_t0_red ac_t0_blue ac_cs2 ac_common ac_sim)
    target_include_directories(cs2_radar_t0 PRIVATE "${RADAR_SOURCE_DIR}/adapters")
  else()
    target_link_libraries(cs2_radar_t0 PRIVATE ac_pipeline ac_t0_red ac_t0_blue ac_cs2 ac_common ac_sim)
  endif()
  target_link_libraries(cs2_radar_t1 PRIVATE ac_t1_red ac_t0_red ac_t1_blue ac_cs2 ac_common ac_sim)
  target_link_libraries(cs2_radar_t2 PRIVATE ac_t2_red ac_t2_blue ac_cs2 ac_common ac_sim)
  target_link_libraries(cs2_radar_t3 PRIVATE ac_t3_red ac_t3_blue ac_cs2 ac_common ac_sim ac_depth)
  target_link_libraries(cs2_radar_t4 PRIVATE ac_t4_red ac_t4_blue ac_t0_red ac_t0_blue ac_cs2 ac_common ac_sim ac_depth ac_server)
endif()
if(LR_BUILD_DUELS)
  foreach(tier 0 1 2 3 4)
    lr_exe(duel_t${tier} apps/demos/duel_t${tier}.cpp ac_t${tier}_red ac_t${tier}_blue)
  endforeach()
  target_link_libraries(duel_t1 PRIVATE ac_t0_red ac_t0_blue)
  target_link_libraries(duel_t2 PRIVATE ac_t0_red ac_t0_blue)
  target_link_libraries(duel_t3 PRIVATE ac_t2_red ac_t0_red ac_server)
  target_link_libraries(duel_t4 PRIVATE ac_server)
endif()
if(TARGET ac_strategies)
  lr_exe(strategy_lab scenarios/strategies/framework/main.cpp ac_strategies)
  if(TARGET full_prototype)
    target_link_libraries(full_prototype PRIVATE ac_strategies)
  endif()
  if(LR_BUILD_DUELS)
    lr_exe(evasion_lab apps/demos/evasion_lab.cpp ac_strategies)
    lr_exe(features_lab apps/demos/features_lab.cpp ac_strategies)
    lr_exe(ops_lab apps/demos/ops_lab.cpp ac_strategies)
    lr_exe(structural_lab apps/demos/structural_lab.cpp ac_strategies)
  endif()
endif()

set(LR_UPDATE_CS2_SIGS_PY "${RADAR_SOURCE_DIR}/scripts/update_cs2_signatures.py")
if(EXISTS "${LR_UPDATE_CS2_SIGS_PY}")
  add_custom_target(update_cs2_signatures
    COMMAND ${CMAKE_COMMAND} -E env PYTHONUTF8=1 python "${LR_UPDATE_CS2_SIGS_PY}"
            --out-dir "${RADAR_SOURCE_DIR}/data/cs2"
    WORKING_DIRECTORY "${RADAR_SOURCE_DIR}"
    COMMENT "Updating CS2 offsets/signatures from a2x/cs2-dumper"
    VERBATIM)
  foreach(_cs2_demo live_radar radar_t0)
    if(TARGET ${_cs2_demo})
      add_custom_command(TARGET ${_cs2_demo} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${_cs2_demo}>/data/cs2"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RADAR_SOURCE_DIR}/data/cs2/offsets_snapshot.json" "$<TARGET_FILE_DIR:${_cs2_demo}>/data/cs2/offsets_snapshot.json"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RADAR_SOURCE_DIR}/data/cs2/offsets_snapshot.json" "$<TARGET_FILE_DIR:${_cs2_demo}>/offsets_snapshot.json"
        COMMENT "Install CS2 offsets_snapshot.json next to ${_cs2_demo}"
        VERBATIM)
    endif()
  endforeach()
endif()

add_custom_target(stealth-check
  COMMAND ${CMAKE_COMMAND} -E echo "=== EDUCATIONAL AUDIT ==="
  COMMAND ${CMAKE_COMMAND} -E echo "Educational audit: review binaries for detectable string patterns."
  COMMENT "Educational audit only; no operational commands are run")
