#include "blue_example.hpp"

#include <string>

namespace examples::hwid_spoof {

BlueResult detect(sim::World& w) {
  BlueResult r;
  int reasons = 0;
  if (w.trust.hwid.find("disk:LAB-NEW") != std::string::npos) ++reasons;
  if (w.trust.hwid.find("smbios:CCC") != std::string::npos) ++reasons;
  if (w.accounts.size() >= 2 && w.accounts[0].payment_fp == w.accounts[1].payment_fp &&
      w.accounts[0].hwid != w.accounts[1].hwid) ++reasons;
  r.detected = reasons >= 2;
  r.mitigated = reasons >= 3;
  if (r.mitigated) w.ranked_access_denied = true;
  r.detail = "identity-consistency reasons=" + std::to_string(reasons);
  return r;
}

}  // namespace examples::hwid_spoof
