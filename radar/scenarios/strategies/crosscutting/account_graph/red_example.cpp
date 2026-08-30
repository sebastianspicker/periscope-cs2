#include "red_example.hpp"
namespace examples::account_graph {
RedResult apply(sim::World& w) {
  RedResult r;
  r.actor_pid = w.spawn("seller-panel.exe");
  w.vpn_proxy_active = true;
  w.add_account(sim::Account{"smurf_a", "hw_new", "pay_seller", "ip_vpn_1", 2});
  w.add_account(sim::Account{"smurf_b", "hw_new2", "pay_seller", "ip_vpn_1", 5});
  w.add_account(sim::Account{"banned_main", "hw_old", "pay_seller", "ip_vpn_2", 12});
  r.accounts = static_cast<int>(w.accounts.size());
  r.achieved = r.accounts >= 3;
  r.detail = "account_graph red accounts=" + std::to_string(r.accounts) + " shared_payment=pay_seller vpn=1";
  w.note(r.detail);
  return r;
}
}  // namespace examples::account_graph
