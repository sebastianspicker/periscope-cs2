// RED example implementation for strategy `wfp_ndis_filter`.
// Multi-step World scars: wfp_ndis_filter residual + telemetry-filter.sys driver.

#include "red_example.hpp"

#include <cstdio>

namespace examples::wfp_ndis_filter {

RedResult apply(sim::World& w) {
  RedResult r{};
  r.detail = "WFP/NDIS precondition failed";

  std::printf("[red:wfp_ndis_filter] require filter residual not already active\n");
  if (w.wfp_ndis_filter) {
    return r;
  }

  std::printf("[red:wfp_ndis_filter] set WFP/NDIS filter residual scar\n");
  w.wfp_ndis_filter = true;
  if (!w.wfp_ndis_filter) {
    r.detail = "failed to set wfp_ndis_filter";
    return r;
  }
  ++r.steps;

  std::printf("[red:wfp_ndis_filter] load telemetry-filter.sys lab driver\n");
  w.load_driver({"telemetry-filter.sys", "filter-sha", "Unknown", false, false,
                 false, false, false});
  if (w.drivers.empty() || w.drivers.back().name != "telemetry-filter.sys") {
    r.detail = "telemetry-filter.sys registration failed";
    return r;
  }
  ++r.steps;

  std::printf("[red:wfp_ndis_filter] verify filter residual and driver presence\n");
  if (!w.wfp_ndis_filter || w.drivers.empty()) {
    r.detail = "network filter residual verification failed";
    return r;
  }
  ++r.steps;

  r.achieved = true;
  r.detail = "network filter registration residual verified";
  w.note(r.detail);
  std::printf("[red:wfp_ndis_filter] %s (%d steps)\n", r.detail.c_str(), r.steps);
  return r;
}

RedResult run_red(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "wfp_ndis_filter",
         "Register a packet filter surface.");
  auto r = apply(w);
  n.say(sim::Side::Red, r.detail);
  return r;
}

}  // namespace examples::wfp_ndis_filter
