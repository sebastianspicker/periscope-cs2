// proto_log.hpp — lightweight protocol/log helpers for tier demos and tests.

#pragma once

#include "ac/risk_score.hpp"
#include "ac/telemetry.hpp"
#include "ac/types.hpp"

#include <cstdio>
#include <string_view>

namespace ac {

inline void proto_banner(std::string_view side, std::string_view tier,
                         std::string_view title) {
  std::printf("\n======== [%.*s %.*s] %.*s ========\n",
              static_cast<int>(side.size()), side.data(),
              static_cast<int>(tier.size()), tier.data(),
              static_cast<int>(title.size()), title.data());
}

inline void proto_status(std::string_view step, Status s) {
  std::printf("  %-28s -> %s\n", step.data(), to_string(s).data());
}

inline void proto_line(std::string_view k, std::string_view v) {
  std::printf("  %-28s %.*s\n", k.data(), static_cast<int>(v.size()), v.data());
}

inline void proto_kv(std::string_view k, double v) {
  std::printf("  %-28s %.2f\n", k.data(), v);
}

inline void proto_kv(std::string_view k, std::size_t v) {
  std::printf("  %-28s %zu\n", k.data(), v);
}

inline void proto_kv(std::string_view k, bool v) {
  std::printf("  %-28s %s\n", k.data(), v ? "true" : "false");
}

inline void proto_events(const MemoryTelemetrySink& sink) {
  std::printf("  telemetry events (%zu):\n", sink.events().size());
  for (const auto& e : sink.events()) {
    std::printf("    - kind=%s tier=%s risk=+%.1f detail=%s\n",
                to_string(e.kind).data(), to_string(e.related_tier).data(),
                e.risk_delta, e.detail.c_str());
  }
}

inline void proto_risk(const RiskState& rs) {
  std::printf("  risk score=%.2f events=%d kinds=%d multi=%s block=%s overwatch=%s\n",
              rs.score, rs.event_count, rs.distinct_kinds,
              rs.multi_signal ? "true" : "false",
              rs.block_ranked ? "true" : "false",
              rs.flag_overwatch ? "true" : "false");
  for (const auto& r : rs.reasons) {
    std::printf("    reason: %s\n", r.c_str());
  }
}


}  // namespace ac
