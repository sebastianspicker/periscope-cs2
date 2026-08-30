#include "blue_example.hpp"

#include <sstream>
#include <string>
#include <unordered_map>

namespace examples::report_velocity {

BlueResult detect(sim::World& w) {
  BlueResult r;

  int same_pay_high = 0;
  int report_sum = 0;
  std::unordered_map<std::string, int> ip_high;
  for (const auto& a : w.accounts) {
    report_sum += a.reports;
    if (a.reports >= 20) {
      if (a.payment_fp == "pay_v") ++same_pay_high;
      if (!a.ip_class.empty()) ++ip_high[a.ip_class];
    }
  }
  r.high_report = same_pay_high;

  // Reason 1: multiple high-report accounts sharing payment fingerprint.
  if (same_pay_high >= 2) {
    r.reasons.emplace_back("shared payment_fp among high-report accounts");
  }

  // Reason 2: elevated total report velocity across graph.
  if (report_sum >= 50) {
    r.reasons.emplace_back("aggregate report_sum threshold exceeded");
  }

  // Reason 3: IP class concentration among high-report accounts.
  for (const auto& [ip, count] : ip_high) {
    (void)ip;
    if (count >= 2) {
      r.reasons.emplace_back("shared ip_class among high-report accounts");
      break;
    }
  }

  // Reason 4: farm coordination residual (VPN/proxy active).
  if (w.vpn_proxy_active && same_pay_high >= 2) {
    r.reasons.emplace_back("vpn_proxy with report-farm cluster");
  }

  r.signals = static_cast<int>(r.reasons.size());
  r.detected = r.signals >= 2;
  r.mitigated = r.detected;
  if (r.mitigated) w.overwatch_queued = true;

  std::ostringstream oss;
  oss << "report_velocity blue signals=" << r.signals
      << " high_report_accounts=" << same_pay_high
      << " report_sum=" << report_sum
      << " overwatch=" << (w.overwatch_queued ? 1 : 0);
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}

}  // namespace examples::report_velocity
