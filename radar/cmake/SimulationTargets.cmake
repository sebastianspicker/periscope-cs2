set(_simulation "${RADAR_SOURCE_DIR}/src/simulation")
set(_labs "${RADAR_SOURCE_DIR}/src/lab_components/lab")

lr_library(ac_sim
  "${_simulation}/sim/donor.cpp"
  "${_simulation}/sim/evasion_master.cpp"
  "${_simulation}/sim/fixture.cpp"
  "${_simulation}/sim/handle_table.cpp"
  "${_simulation}/sim/memory_reader.cpp"
  "${_simulation}/sim/overlay_detection.cpp"
  "${_simulation}/sim/world.cpp"
  "${_simulation}/sim/world_lab_memory.cpp"
  "${_simulation}/ac_sim/accept_gate.cpp"
  "${_simulation}/ac_sim/api_integrity.cpp"
  "${_simulation}/ac_sim/batch_engine.cpp"
  "${_simulation}/ac_sim/behavioral_filter.cpp"
  "${_simulation}/ac_sim/decoy_render.cpp"
  "${_simulation}/ac_sim/disguise.cpp"
  "${_simulation}/ac_sim/dll_watch.cpp"
  "${_simulation}/ac_sim/etl.cpp"
  "${_simulation}/ac_sim/forensic.cpp"
  "${_simulation}/ac_sim/health_ladder.cpp"
  "${_simulation}/ac_sim/self_verify.cpp"
  "${_simulation}/ac_sim/system_normalize.cpp"
  "${_simulation}/ac_sim/temporal.cpp"
  "${_simulation}/ac_sim/xorshift.cpp")
target_include_directories(ac_sim PUBLIC "${_simulation}" PRIVATE "${RADAR_GENERATED_DIR}")
# Simulation is fake-only. Real platform observation belongs to adapters and
# is composed by real applications, never linked into the simulation graph.
target_link_libraries(ac_sim PUBLIC ac_common)

lr_library(ac_fps
  "${_simulation}/fps/lab_bridge.cpp"
  "${_simulation}/fps/map.cpp"
  "${_simulation}/fps/scenario.cpp"
  "${_simulation}/fps/scenario_combat.cpp")
target_include_directories(ac_fps PUBLIC "${_simulation}")
target_link_libraries(ac_fps PUBLIC ac_common ac_sim)

lr_library(ac_cs2_signatures
  "${_simulation}/cs2/signatures.cpp"
  "${_simulation}/cs2/signatures_data.cpp"
  "${_simulation}/cs2/signatures_scan.cpp")
target_include_directories(ac_cs2_signatures PUBLIC "${_simulation}")
target_link_libraries(ac_cs2_signatures PUBLIC ac_common)
lr_library(ac_cs2
  "${_simulation}/cs2/ac_evasion.cpp"
  "${_simulation}/cs2/diagnostic_orchestrator.cpp"
  "${_simulation}/cs2/diagnostic_sensors.cpp"
  "${_simulation}/cs2/diagnostic_stack_analysis.cpp"
  "${_simulation}/cs2/diagnostic_system.cpp"
  "${_simulation}/cs2/entities.cpp"
  "${_simulation}/cs2/pattern_analysis_sensors.cpp"
  "${_simulation}/cs2/schema.cpp"
  "${_simulation}/cs2/simulator.cpp"
  "${_simulation}/cs2/simulator_plant.cpp")
target_include_directories(ac_cs2 PUBLIC "${_simulation}")
target_link_libraries(ac_cs2 PUBLIC ac_sim ac_common ac_cs2_signatures)

lr_library(ac_lab_scanner "${_labs}/aob_scanner.cpp")
target_include_directories(ac_lab_scanner PUBLIC "${RADAR_SOURCE_DIR}/src/lab_components")
target_link_libraries(ac_lab_scanner PUBLIC ac_cs2_signatures ac_sim)
lr_library(ac_lab
  "${_labs}/api_table.cpp"
  "${_labs}/cheat_sig_db.cpp"
  "${_labs}/disguise_engine.cpp"
  "${_labs}/draw_list.cpp"
  "${_labs}/fixture_process.cpp"
  "${_labs}/lab_memory.cpp"
  "${_labs}/lab_session.cpp"
  "${_labs}/scan_detector.cpp")
target_include_directories(ac_lab PUBLIC "${RADAR_SOURCE_DIR}/src/lab_components")
target_link_libraries(ac_lab PUBLIC ac_common ac_sim ac_lab_scanner ac_cs2_signatures)
