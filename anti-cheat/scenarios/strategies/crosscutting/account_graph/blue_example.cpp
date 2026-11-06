#include "blue_example.hpp"
namespace examples::account_graph {
BlueResult detect(sim::World& w) {
  BlueResult r;
  int same_pay = 0;
  for (const auto& a : w.accounts) if (a.payment_fp == "pay_seller") ++same_pay;
  r.linked = same_pay;
  r.detected = same_pay >= 2 && (w.vpn_proxy_active || same_pay >= 3);
  if (r.detected) { r.mitigated = true; w.overwatch_queued = true; }
  r.detail = "account_graph blue linked=" + std::to_string(same_pay) + " vpn=" + std::to_string(w.vpn_proxy_active?1:0);
  w.note(r.detail);
  return r;
}
}  // namespace examples::account_graph
