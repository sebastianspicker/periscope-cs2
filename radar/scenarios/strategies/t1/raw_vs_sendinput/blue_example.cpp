#include "blue_example.hpp"

#include <algorithm>
#include <cstdio>

namespace examples::raw_vs_sendinput {

BlueResult detect(sim::World& w) {
  BlueResult r{};
  int raw_count = 0;
  int injected_count = 0;

  std::printf("[blue:raw_vs_sendinput] sensor 1: explicit mix flag\n");
  const bool flag_hit = w.raw_sendinput_mixed;
  if (flag_hit) {
    r.reasons.emplace_back("raw_sendinput_mixed scar is set");
  }

  std::printf("[blue:raw_vs_sendinput] sensor 2: provenance inventory\n");
  for (const auto& e : w.inputs) {
    if (e.source == "raw_hid") ++raw_count;
    if (e.source == "injected") ++injected_count;
  }
  const bool mixed_sources = raw_count > 0 && injected_count > 0;
  if (raw_count > 0) {
    r.reasons.emplace_back("raw HID input events observed");
  }
  if (injected_count > 0) {
    r.reasons.emplace_back("injected SendInput-style events observed");
  }
  if (mixed_sources) {
    r.reasons.emplace_back("raw HID and injected input mixed in same window");
  }

  r.signals = static_cast<int>(r.reasons.size());
  r.risk = std::min(1.0, r.signals * 0.22);
  r.detected = r.signals >= 2;
  r.mitigated = r.detected;
  if (r.risk >= 0.8) w.ranked_access_denied = true;
  r.detail = "flag=" + std::string(flag_hit ? "1" : "0") +
             " raw=" + std::to_string(raw_count) +
             " injected=" + std::to_string(injected_count);
  std::printf("[blue:raw_vs_sendinput] %s detected=%d\n", r.detail.c_str(),
              r.detected);
  return r;
}

}  // namespace examples::raw_vs_sendinput
