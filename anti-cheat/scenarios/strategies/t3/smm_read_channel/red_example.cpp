#include "red_example.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace examples::smm_read_channel {

RedResult apply(sim::World& w) {
  RedResult result{false, 0, false, ""};
  const auto game = w.game_pid();
  const auto* game_process = w.proc(game);
  if (game == 0 || game_process == nullptr) {
    return {false, result.steps, false, "game process is unavailable"};
  }
  ++result.steps;

  // The lab model requires platform trust to be cleared before its coordinator HV starts.
  w.trust.vbs = false;
  w.trust.hvci = false;
  w.trust.hvci_enabled = false;
  if (!w.try_start_personal_hv("LabSmmCoordinator")) {
    return {false, result.steps, false, "personal HV coordinator was blocked"};
  }
  ++result.steps;

  const auto bridge = w.spawn("smm-bridge-ui.exe");
  w.load_driver({"smmbridge.sys", "sha-smmbridge", "lab-self-signed", false,
                 false, false, true, true, 10});
  w.create_device({"\\Device\\SmmBridge", "smmbridge.sys", true});
  if (bridge == 0 || w.proc(bridge) == nullptr) {
    return {false, result.steps, false, "SMM bridge actor was unavailable"};
  }
  ++result.steps;

  // Educational residual only: this represents a pre-existing firmware handler.
  w.smm_read_channel_planted = true;
  w.smm_residual = true;
  w.trust.unexpected_efi_entry = true;
  w.trust.efi_entry_name = "lab-smm-channel";
  if (!w.smm_read_channel_planted) {
    return {false, result.steps, false, "SMM channel residual was not recorded"};
  }
  ++result.steps;

  std::vector<std::uint8_t> entity_bytes;
  const bool entities_read = w.hv_read(bridge, game, game_process->base, 4,
                                       entity_bytes);
  if (entities_read) {
    ++w.smm_read_ops;
  }
  ++result.steps;

  const bool no_game_handle = w.handles_to(game).empty();
  result.bypasses_hv = w.trust.personal_hv_active && w.smm_read_channel_planted;
  result.achieved = w.smm_read_channel_planted && entities_read &&
                    w.smm_read_ops > 0 && no_game_handle;
  result.detail = "SMM lab channel planted=" +
                  std::to_string(w.smm_read_channel_planted) +
                  " reads=" + std::to_string(w.smm_read_ops) +
                  " no_game_handle=" + std::to_string(no_game_handle) +
                  " bypasses_hv=" + std::to_string(result.bypasses_hv);
  w.note(result.detail);
  return result;
}

}  // namespace examples::smm_read_channel
