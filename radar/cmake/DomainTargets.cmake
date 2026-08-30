set(_domain "${RADAR_SOURCE_DIR}/src/domain")
set(_application "${RADAR_SOURCE_DIR}/src/application")
set(_labs "${RADAR_SOURCE_DIR}/src/lab_components")
include("${CMAKE_CURRENT_LIST_DIR}/TeamSources.cmake")

# Keep the domain independently buildable: it may not import simulation,
# application, lab, scenario, CS2/FPS, or real-adapter implementation headers.
# The explicit list intentionally avoids a directory glob so new domain files
# must be reviewed and added to a target manifest.
set(LR_DOMAIN_FILES
  "${_domain}/ac/memory_backend.hpp"
  "${_domain}/ac/proto_log.hpp"
  "${_domain}/ac/risk_score.cpp"
  "${_domain}/ac/risk_score.hpp"
  "${_domain}/ac/telemetry.cpp"
  "${_domain}/ac/telemetry.hpp"
  "${_domain}/ac/types.cpp"
  "${_domain}/ac/types.hpp"
  "${_domain}/server/ban_correlator.cpp"
  "${_domain}/server/ban_correlator.hpp"
  "${_domain}/server/info_advantage.cpp"
  "${_domain}/server/info_advantage.hpp"
  "${_domain}/server/interest_mgmt.cpp"
  "${_domain}/server/interest_mgmt.hpp")
foreach(_domain_file IN LISTS LR_DOMAIN_FILES)
  file(READ "${_domain_file}" _domain_contents)
  if(_domain_contents MATCHES "#include[ \\t]+[<\"](sim|ac_sim|cs2|fps|lab|adapters|real|application|scenarios)/")
    message(FATAL_ERROR "Domain source imports a non-domain layer: ${_domain_file}")
  endif()
endforeach()
unset(_domain_contents)
unset(_domain_file)

lr_library(ac_common
  "${_domain}/ac/risk_score.cpp"
  "${_domain}/ac/telemetry.cpp"
  "${_domain}/ac/types.cpp")
target_include_directories(ac_common PUBLIC "${_domain}")
lr_library(ac_strategy_core
  "${_application}/strategies/catalog_util.cpp"
  "${_application}/strategies/framework_meta.cpp"
  "${_application}/strategies/multi_reason.cpp"
  "${_application}/strategies/pair_util.cpp"
  "${_application}/strategies/scar_sensors.cpp"
  "${_application}/strategies/strategy_support.cpp")
target_include_directories(ac_strategy_core PUBLIC "${_application}")
target_link_libraries(ac_strategy_core PUBLIC ac_common ac_sim)

lr_library(ac_blue
  "${_application}/detection/blue/blue_system.cpp"
  "${_application}/detection/blue/blue_system_behavior_post.cpp"
  "${_application}/detection/blue/blue_system_handle_module.cpp"
  "${_application}/detection/blue/blue_system_memory_process.cpp")
target_include_directories(ac_blue PUBLIC "${_application}/detection")
target_link_libraries(ac_blue PUBLIC ac_common ac_sim)
lr_library(ac_server
  "${_domain}/server/ban_correlator.cpp"
  "${_domain}/server/info_advantage.cpp"
  "${_domain}/server/interest_mgmt.cpp")
target_include_directories(ac_server PUBLIC "${_domain}")
target_link_libraries(ac_server PUBLIC ac_common)
lr_library(ac_depth
  "${_application}/analysis/depth/handle_multisample.cpp"
  "${_application}/analysis/depth/leakage_scorer.cpp"
  "${_application}/analysis/depth/multi_invariant_scorer.cpp"
  "${_application}/analysis/depth/residual_research.cpp"
  "${_application}/analysis/depth/seller_fusion.cpp"
  "${_application}/analysis/depth/trust_aggregator.cpp")
target_include_directories(ac_depth PUBLIC "${_application}/analysis")
target_link_libraries(ac_depth PUBLIC ac_sim ac_server ac_common)

# Tier teams are reusable lab components, deliberately independent of lesson files.
function(lr_team target tier)
  lr_library(${target} ${LR_${tier}_SOURCES})
  target_include_directories(${target} PUBLIC "${_labs}/teams")
endfunction()
lr_team(ac_t0_red t0_red)
target_link_libraries(ac_t0_red PUBLIC ac_common ac_lab ac_sim)
lr_team(ac_t0_blue t0_blue)
target_link_libraries(ac_t0_blue PUBLIC ac_common ac_sim)
lr_team(ac_t1_red t1_red)
target_link_libraries(ac_t1_red PUBLIC ac_t0_red)
lr_team(ac_t1_blue t1_blue)
target_link_libraries(ac_t1_blue PUBLIC ac_t0_blue ac_sim)
lr_team(ac_t2_red t2_red)
target_link_libraries(ac_t2_red PUBLIC ac_t0_red)
lr_team(ac_t2_blue t2_blue)
target_link_libraries(ac_t2_blue PUBLIC ac_common ac_sim ac_t2_red)
lr_team(ac_t3_red t3_red)
target_link_libraries(ac_t3_red PUBLIC ac_t2_red ac_t1_red ac_t0_red)
lr_team(ac_t3_blue t3_blue)
target_link_libraries(ac_t3_blue PUBLIC ac_t2_blue ac_common ac_sim ac_depth)
lr_team(ac_t4_red t4_red)
target_link_libraries(ac_t4_red PUBLIC ac_common ac_sim)
lr_team(ac_t4_blue t4_blue)
target_link_libraries(ac_t4_blue PUBLIC ac_common ac_sim ac_depth ac_server)
