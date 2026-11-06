// BLUE example implementation for this strategy pair.
// Multi-reason sensors on World scars; educational detect/mitigate path.

#include "blue_example.hpp"

#include <algorithm>

namespace examples::dse_testsign {

BlueResult detect(sim::World& w) {
  BlueResult r{};
  if (w.trust.test_signing) r.reasons.push_back("test-signing mode is enabled");
  if (!w.trust.dse_enforced) r.reasons.push_back("driver signature enforcement is disabled");
  if (!w.trust.hvci_enabled) r.reasons.push_back("HVCI protection is disabled");
  for (const auto& d : w.drivers) if (!d.is_ac && d.provides_mem_rw) { r.reasons.push_back("non-AC driver exposes memory read/write capability"); break; }
  for (const auto& d : w.devices) if (d.mem_rw_ioctl) { r.reasons.push_back("memory-capable device object is present"); break; }
  r.signals = static_cast<int>(r.reasons.size());
  r.risk = std::min(1.0, r.signals * 0.24);
  r.detected = r.signals >= 2;
  if (r.risk >= 0.72) w.ranked_access_denied = true;
  return r;
}

}  // namespace examples::dse_testsign
