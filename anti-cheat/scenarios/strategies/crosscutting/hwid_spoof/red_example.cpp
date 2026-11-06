#include "red_example.hpp"

namespace examples::hwid_spoof {

RedResult apply(sim::World& w) {
  RedResult r;
  r.actor_pid = w.spawn("identity-profile-lab.exe");
  w.add_account({"account-before", "disk:AAA|mac:BBB|smbios:CCC", "pay:stable", "residential", 0});
  w.spoof_hwid("disk:LAB-NEW|mac:LAB-NEW|smbios:CCC");
  w.add_account({"account-now", w.trust.hwid, "pay:stable", "residential", 0});
  r.achieved = true;
  r.detail = "simulated identity profile rotated with firmware identity left unchanged";
  return r;
}

}  // namespace examples::hwid_spoof
