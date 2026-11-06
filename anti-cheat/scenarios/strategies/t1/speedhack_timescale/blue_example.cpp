// BLUE example implementation for this strategy pair.
// Multi-reason sensors on World scars; educational detect/mitigate path.

#include "blue_example.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace examples::speedhack_timescale {

namespace {
constexpr double kEpsilon = 1e-3;
}  // namespace

BlueResult detect(sim::World& w) {
  BlueResult r{};

  const double delta = std::abs(w.time_scale - 1.0);
  const bool scale_hit = delta > kEpsilon;
  const bool timing_spoofed = w.trust.timing_spoofed;
  // Independent clock disagree: scale scar and trust timing leaf both present.
  const bool clocks_disagree = scale_hit && timing_spoofed;

  if (scale_hit) {
    r.reasons.emplace_back("time_scale deviates from identity beyond epsilon");
  }
  if (timing_spoofed) {
    r.reasons.emplace_back("host trust marks timing as spoofed");
  }
  if (clocks_disagree) {
    r.reasons.emplace_back("independent clock sources disagree on host time");
  }

  r.signals = static_cast<int>(r.reasons.size());
  r.risk = std::min(1.0, r.signals * 0.20);
  r.detected = r.signals >= 2;
  r.mitigated = r.detected;
  r.detail = std::to_string(r.signals) + " signals risk=" + std::to_string(r.risk) +
             " delta=" + std::to_string(delta) +
             " timing_spoofed=" + std::to_string(timing_spoofed ? 1 : 0);
  std::printf("[blue:speedhack_timescale] %s\n", r.detail.c_str());
  return r;
}

}  // namespace examples::speedhack_timescale
