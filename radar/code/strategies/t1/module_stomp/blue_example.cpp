#include "blue_example.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

namespace examples::module_stomp {

BlueResult detect(sim::World& w) {
  BlueResult r{};
  const auto* g = w.proc(w.game_pid());
  int dirty = 0;

  std::printf("[blue:module_stomp] sensor 1: known-module text integrity\n");
  if (g != nullptr) {
    for (const auto& m : g->modules) {
      const bool known = m.name.find("client") != std::string::npos ||
                         m.name.find("game") != std::string::npos ||
                         m.name.find(".exe") != std::string::npos ||
                         m.name.find(".dll") != std::string::npos;
      if (known && m.text_hash != "clean") {
        ++dirty;
        r.reasons.emplace_back("module " + m.name +
                               " text_hash != clean (" + m.text_hash + ")");
      }
      if (m.headers_erased) {
        ++dirty;
        r.reasons.emplace_back("module " + m.name + " headers erased");
      }
    }
  }
  if (dirty == 0 && g != nullptr) {
    for (const auto& m : g->modules) {
      if (m.text_hash != "clean") {
        ++dirty;
        r.reasons.emplace_back("module integrity differs from baseline: " + m.name);
      }
    }
  }

  std::printf("[blue:module_stomp] sensor 2: stomped hash signature\n");
  if (g != nullptr) {
    for (const auto& m : g->modules) {
      if (m.text_hash == "stomped") {
        r.reasons.emplace_back("explicit stomped text_hash on " + m.name);
        break;
      }
    }
  }

  // Deduplicate reason count for signals
  r.signals = static_cast<int>(r.reasons.size());
  r.risk = std::min(1.0, r.signals * 0.28);
  r.detected = (dirty + r.signals) >= 2;
  r.mitigated = r.detected;
  if (r.risk >= 0.8) w.ranked_access_denied = true;
  r.detail = "dirty=" + std::to_string(dirty) +
             " signals=" + std::to_string(r.signals);
  std::printf("[blue:module_stomp] %s detected=%d\n", r.detail.c_str(), r.detected);
  return r;
}

}  // namespace examples::module_stomp
