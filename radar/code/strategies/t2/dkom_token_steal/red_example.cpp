#include "red_example.hpp"

#include <algorithm>
#include <cstdio>

namespace examples::dkom_token_steal {

RedResult apply(sim::World& w) {
  RedResult result{false, 0, false,
                   "DKOM token-steal precondition failed: game unavailable"};
  const auto game = w.game_pid();
  const auto* game_process = w.proc(game);
  if (!game || !game_process) {
    return result;
  }
  const auto game_base = game_process->base;

  const auto reader = w.spawn("syshelper.exe");
  if (!w.proc(reader)) {
    return result;
  }
  ++result.steps;

  w.load_driver({"dkom-token-lab.sys", "dkom-token-lab-sha", "LabVendor",
                 false, false, false, false, false});
  ++result.steps;

  w.dkom_token_stolen = true;
  w.dkom_token_source_pid = 4;
  w.token_bypasses_handle_acls = true;
  ++result.steps;

  const auto full_access = sim::AccessMask::VmRead | sim::AccessMask::VmWrite |
                           sim::AccessMask::VmOperation;
  if (!w.open_process(reader, game, full_access, false)) {
    return result;
  }
  ++result.steps;

  const auto read = w.read_mem(reader, game, game_base, 4, true);
  if (read.status != ac::Status::Ok || read.bytes.size() != 4) {
    return result;
  }
  ++result.steps;

  result.handle_bypasses_acls = w.token_bypasses_handle_acls;
  result.achieved = w.dkom_token_stolen && w.dkom_token_source_pid == 4 &&
                    result.handle_bypasses_acls;
  result.detail =
      "DKOM SYSTEM token scar and full-access game handle verified";
  std::printf("[red/dkom_token_steal] %s (%d steps)\n",
              result.detail.c_str(), result.steps);
  return result;
}

}  // namespace examples::dkom_token_steal
