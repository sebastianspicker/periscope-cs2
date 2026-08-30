#include "red_example.hpp"

namespace examples::skin_changer {

RedResult apply(sim::World& w) {
  int steps = 0;
  // Phase 1: rotate paint-kit / inventory convar CRC away from expected.
  w.diagnostic_state.convar_crc_mode1 = 0xDEADBEEF;
  ++steps;
  // Phase 2: mismatch client.dll PE timestamp (inventory stamp desync).
  w.diagnostic_state.pe_timestamp_client_dll =
      w.diagnostic_state.expected_pe_timestamp_client_dll ^ 0x1800A000u;
  ++steps;
  // Phase 3: second inventory surface — mode2 CRC also diverges (econ attrs).
  w.diagnostic_state.convar_crc_mode2 =
      w.diagnostic_state.expected_convar_crc_mode2 ^ 0x55AAu;
  if (w.diagnostic_state.expected_convar_crc_mode2 == 0) {
    w.diagnostic_state.expected_convar_crc_mode2 = 0x11111111u;
    w.diagnostic_state.convar_crc_mode2 = 0x22222222u;
  }
  ++steps;
  w.note("skin_changer: paint kit / inventory attrs modified across 3 surfaces");
  return {true, steps, "skin_changer: 3 attributes modified"};
}

}  // namespace examples::skin_changer
