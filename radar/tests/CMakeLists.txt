# Host tests for portable logic only. Nothing here links a real adapter, so the
# suite runs on every platform with the default configuration.
# `enable_testing()` is called by the parent CMakeLists before this directory.

set(_HOST_TESTS
  stack_posture_test
  radar_snapshot_test
  radar_sampling_test
  radar_ring_test
  radar_geometry_test
  radar_offsets_test
  radar_scatter_test
  radar_los_test
  radar_webradar_test
  radar_tri_loader_test)
foreach(target ${_HOST_TESTS})
  add_executable(${target} "${RADAR_SOURCE_DIR}/tests/${target}.cpp")
  target_link_libraries(${target} PRIVATE lr_build_options)
  target_include_directories(${target} PRIVATE "${RADAR_SOURCE_DIR}/adapters")
  add_test(NAME ${target} COMMAND ${target})
endforeach()

# The radar-domain ledger tests additionally link the pure radar snapshot /
# sampling / projection library (see DomainTargets.cmake).
foreach(target
    radar_snapshot_test
    radar_sampling_test
    radar_ring_test
    radar_geometry_test
    radar_offsets_test
    radar_scatter_test
    radar_los_test
    radar_webradar_test
    radar_tri_loader_test)
  target_link_libraries(${target} PRIVATE radar_snapshot radar_webradar
    radar_collision)
endforeach()
