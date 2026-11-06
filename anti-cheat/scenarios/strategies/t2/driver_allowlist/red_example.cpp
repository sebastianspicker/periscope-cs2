// RED example implementation for strategy `driver_allowlist`.
// Multi-step World scars: allowlisted identity with memory device under ranked policy.

#include "red_example.hpp"
#include "strategies/strategy_support.hpp"

#include <cstdio>

namespace examples::driver_allowlist {

RedResult apply(sim::World& w) {
  RedResult r{};
  r.detail = "allowlist precondition failed: ranked policy disabled";

  std::printf("[red:driver_allowlist] enable ranked strict driver allowlist\n");
  if (!w.ranked_strict_driver_allowlist) {
    w.ranked_strict_driver_allowlist = true;
  }
  w.driver_allowlist_sha = {"approved-demo-sha"};
  if (!w.ranked_strict_driver_allowlist || w.driver_allowlist_sha.empty()) {
    r.detail = "failed to configure ranked allowlist policy";
    return r;
  }
  ++r.steps;

  std::printf("[red:driver_allowlist] load allowlisted approved-looking.sys\n");
  w.load_driver({"approved-looking.sys", "approved-demo-sha", "TrustedVendor",
                 false, false, false, false, true});
  const bool listed = support::has_driver(w, "approved-looking.sys") &&
                      !w.drivers.empty() &&
                      w.drivers.back().sha256 == w.driver_allowlist_sha.front();
  if (!listed) {
    r.detail = "allowlisted driver registration failed";
    return r;
  }
  ++r.steps;

  std::printf("[red:driver_allowlist] expose ApprovedMemRw device and verify\n");
  w.create_device({"\\\\.\\ApprovedMemRw", "approved-looking.sys", true});
  if (!support::has_device(w, "\\\\.\\ApprovedMemRw")) {
    r.detail = "ApprovedMemRw device creation failed";
    return r;
  }
  ++r.steps;

  r.achieved = true;
  r.detail = "allowlisted memory-capable driver path verified";
  w.note(r.detail);
  std::printf("[red:driver_allowlist] %s (%d steps)\n", r.detail.c_str(), r.steps);
  return r;
}

RedResult run_red(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "driver_allowlist",
         "Use a policy-approved driver identity.");
  auto r = apply(w);
  n.say(sim::Side::Red, r.detail);
  return r;
}

}  // namespace examples::driver_allowlist
