# Lab domain (ac, server) and lab application components live in the
# anti-cheat tree; the includer must define AC_SOURCE_DIR before including
# this file (radar's root CMakeLists and the anti-cheat stub both do).
# Radar-side paths stay on RADAR_SOURCE_DIR; the whole radar block below is
# guarded so the lab can include this file standalone without radar sources.
set(_ac_domain "${AC_SOURCE_DIR}/src/domain")
set(_ac_application "${AC_SOURCE_DIR}/src/application")
set(_labs "${AC_SOURCE_DIR}/src/lab_components")
include("${CMAKE_CURRENT_LIST_DIR}/TeamSources.cmake")

# Keep the domain independently buildable: it may not import simulation,
# application, lab, scenario, CS2/FPS, or real-adapter implementation headers.
# The explicit list intentionally avoids a directory glob so new domain files
# must be reviewed and added to a target manifest.
set(LR_AC_DOMAIN_FILES
  "${_ac_domain}/ac/memory_backend.hpp"
  "${_ac_domain}/ac/proto_log.hpp"
  "${_ac_domain}/ac/risk_score.cpp"
  "${_ac_domain}/ac/risk_score.hpp"
  "${_ac_domain}/ac/telemetry.cpp"
  "${_ac_domain}/ac/telemetry.hpp"
  "${_ac_domain}/ac/types.cpp"
  "${_ac_domain}/ac/types.hpp"
  "${_ac_domain}/server/ban_correlator.cpp"
  "${_ac_domain}/server/ban_correlator.hpp"
  "${_ac_domain}/server/info_advantage.cpp"
  "${_ac_domain}/server/info_advantage.hpp"
  "${_ac_domain}/server/interest_mgmt.cpp"
  "${_ac_domain}/server/interest_mgmt.hpp")
foreach(_domain_file IN LISTS LR_AC_DOMAIN_FILES)
  file(READ "${_domain_file}" _domain_contents)
  if(_domain_contents MATCHES "#include[ \\t]+[<\"](sim|ac_sim|cs2|fps|lab|adapters|real|application|scenarios)/")
    message(FATAL_ERROR "Domain source imports a non-domain layer: ${_domain_file}")
  endif()
endforeach()
unset(_domain_contents)
unset(_domain_file)

lr_library(ac_common
  "${_ac_domain}/ac/risk_score.cpp"
  "${_ac_domain}/ac/telemetry.cpp"
  "${_ac_domain}/ac/types.cpp")
target_include_directories(ac_common PUBLIC "${_ac_domain}")

if(RADAR_SOURCE_DIR)
  # Radar domain policies (ledger R1-R4, R7, R9-R12): pure, portable, tested.
  # These stay in the radar tree; same purity rule as the lab domain above.
  set(_radar_domain "${RADAR_SOURCE_DIR}/src/domain")
  set(LR_RADAR_DOMAIN_FILES
    "${_radar_domain}/radar/snapshot.hpp"
    "${_radar_domain}/radar/snapshot.cpp"
    "${_radar_domain}/radar/sampling.hpp"
    "${_radar_domain}/radar/snapshot_ring.hpp"
    "${_radar_domain}/radar/projection.hpp"
    "${_radar_domain}/radar/projection.cpp"
    "${_radar_domain}/radar/intervals.hpp"
    "${_radar_domain}/radar/mini_radar.hpp"
    "${_radar_domain}/radar/mini_radar.cpp"
    "${_radar_domain}/radar/offset_overrides.hpp"
    "${_radar_domain}/radar/offset_overrides.cpp"
    "${_radar_domain}/radar/scatter_batch.hpp"
    "${_radar_domain}/radar/scatter_batch.cpp"
    "${_radar_domain}/radar/los_bvh.hpp"
    "${_radar_domain}/radar/los_bvh.cpp")
  foreach(_domain_file IN LISTS LR_RADAR_DOMAIN_FILES)
    file(READ "${_domain_file}" _domain_contents)
    if(_domain_contents MATCHES "#include[ \\t]+[<\"](sim|ac_sim|cs2|fps|lab|adapters|real|application|scenarios)/")
      message(FATAL_ERROR "Domain source imports a non-domain layer: ${_domain_file}")
    endif()
  endforeach()
  unset(_domain_contents)
  unset(_domain_file)

  lr_library(radar_snapshot
    "${_radar_domain}/radar/snapshot.cpp"
    "${_radar_domain}/radar/projection.cpp"
    "${_radar_domain}/radar/mini_radar.cpp"
    "${_radar_domain}/radar/offset_overrides.cpp"
    "${_radar_domain}/radar/scatter_batch.cpp"
    "${_radar_domain}/radar/los_bvh.cpp")
  target_include_directories(radar_snapshot PUBLIC "${_radar_domain}")
  # Web-radar service core (ledger R5): transport-independent, fake-client tested.
  lr_library(radar_webradar
    "${RADAR_SOURCE_DIR}/src/application/radar/webradar_service.cpp")
  target_include_directories(radar_webradar PUBLIC
    "${RADAR_SOURCE_DIR}/src/application" "${_radar_domain}")
  target_link_libraries(radar_webradar PUBLIC radar_snapshot)
  # Map-collision `.tri` file format (ledger R11 portable half).
  lr_library(radar_collision
    "${RADAR_SOURCE_DIR}/src/application/radar/tri_loader.cpp")
  target_include_directories(radar_collision PUBLIC
    "${RADAR_SOURCE_DIR}/src/application" "${_radar_domain}")
  target_link_libraries(radar_collision PUBLIC radar_snapshot)
endif()

lr_library(ac_strategy_core
  "${_ac_application}/strategies/catalog_util.cpp"
  "${_ac_application}/strategies/framework_meta.cpp"
  "${_ac_application}/strategies/multi_reason.cpp"
  "${_ac_application}/strategies/pair_util.cpp"
  "${_ac_application}/strategies/scar_sensors.cpp"
  "${_ac_application}/strategies/strategy_support.cpp")
target_include_directories(ac_strategy_core PUBLIC "${_ac_application}")
target_link_libraries(ac_strategy_core PUBLIC ac_common ac_sim)

lr_library(ac_blue
  "${_ac_application}/detection/blue/blue_system.cpp"
  "${_ac_application}/detection/blue/blue_system_behavior_post.cpp"
  "${_ac_application}/detection/blue/blue_system_handle_module.cpp"
  "${_ac_application}/detection/blue/blue_system_memory_process.cpp")
target_include_directories(ac_blue PUBLIC "${_ac_application}/detection")
target_link_libraries(ac_blue PUBLIC ac_common ac_sim)
lr_library(ac_server
  "${_ac_domain}/server/ban_correlator.cpp"
  "${_ac_domain}/server/info_advantage.cpp"
  "${_ac_domain}/server/interest_mgmt.cpp")
target_include_directories(ac_server PUBLIC "${_ac_domain}")
target_link_libraries(ac_server PUBLIC ac_common)
lr_library(ac_depth
  "${_ac_application}/analysis/depth/handle_multisample.cpp"
  "${_ac_application}/analysis/depth/leakage_scorer.cpp"
  "${_ac_application}/analysis/depth/multi_invariant_scorer.cpp"
  "${_ac_application}/analysis/depth/residual_research.cpp"
  "${_ac_application}/analysis/depth/seller_fusion.cpp"
  "${_ac_application}/analysis/depth/trust_aggregator.cpp")
target_include_directories(ac_depth PUBLIC "${_ac_application}/analysis")
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
