# Host tests for the lab tree. `enable_testing()` is called by the parent
# CMakeLists before this directory.
#
# stack_posture_test audits the real stack's posture policy from the lab side:
# it quote-includes the header-only, portable `real/cs2/posture.hpp` through
# the test-only LR_RADAR_DIR seam (see the root CMakeLists). This is the
# deliberate, documented exception to the lab's "no radar references" rule;
# the architecture checker scopes its no-`real/` scan to anti-cheat/src +
# anti-cheat/scenarios accordingly.

if(AC_HAVE_RADAR)
  add_executable(stack_posture_test "${AC_SOURCE_DIR}/tests/stack_posture_test.cpp")
  target_link_libraries(stack_posture_test PRIVATE lr_build_options)
  # posture.hpp is self-contained (standard headers only), so the only include
  # root needed beyond lr_build_options is radar's adapters tree, which owns
  # the `real/` prefix of the quoted include path.
  target_include_directories(stack_posture_test PRIVATE "${LR_RADAR_DIR}/adapters")
  add_test(NAME stack_posture_test COMMAND stack_posture_test)
endif()

# Registry integrity always runs, radar checkout or not.
add_executable(catalog_test "${AC_SOURCE_DIR}/tests/catalog_test.cpp")
target_link_libraries(catalog_test PRIVATE ac_strategies ac_strategy_core)
add_test(NAME catalog_test COMMAND catalog_test)

# Full catalog smoke run. Every pair must reach a blue verdict (or a red
# failure is now a failure, not a pass).
add_test(NAME strategy_catalog_all COMMAND strategy_lab all --quiet)
