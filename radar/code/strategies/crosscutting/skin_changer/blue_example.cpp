#include "blue_example.hpp"

namespace examples::skin_changer {

BlueResult detect(sim::World& w) {
  BlueResult r;
  if (w.diagnostic_state.convar_crc_mode1 !=
      w.diagnostic_state.expected_convar_crc_mode1) {
    r.violations++;
    r.attrs.push_back("paint_kit");
  }
  if (w.diagnostic_state.pe_timestamp_client_dll !=
      w.diagnostic_state.expected_pe_timestamp_client_dll) {
    r.violations++;
    r.attrs.push_back("client_stamp");
  }
  if (w.diagnostic_state.convar_crc_mode2 !=
      w.diagnostic_state.expected_convar_crc_mode2) {
    r.violations++;
    r.attrs.push_back("econ_mode2");
  }
  // Multi-reason: require >=2 independent inventory/stamp mismatches.
  r.detected = r.violations >= 2;
  r.detail = "violations=" + std::to_string(r.violations);
  return r;
}

}  // namespace examples::skin_changer
