# Host tests for portable logic only. Nothing here links a real adapter, so the
# suite runs on every platform with the default configuration.
# `enable_testing()` is called by the parent CMakeLists before this directory.

set(_HOST_TESTS stack_posture_test)
foreach(target ${_HOST_TESTS})
  add_executable(${target} "${RADAR_SOURCE_DIR}/tests/${target}.cpp")
  target_link_libraries(${target} PRIVATE lr_build_options)
  target_include_directories(${target} PRIVATE "${RADAR_SOURCE_DIR}/adapters")
  add_test(NAME ${target} COMMAND ${target})
endforeach()
