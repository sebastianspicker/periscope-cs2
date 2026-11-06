#include "red_example.hpp"

#include <cstdio>
#include <string>

namespace examples::module_stomp {

RedResult apply(sim::World& w) {
  RedResult r{};
  const auto game = w.game_pid();
  auto* g = w.proc(game);
  std::printf("[red:module_stomp] step 1: validate game arena\n");
  if (game == 0 || g == nullptr) {
    r.detail = "precondition failed: game process missing";
    return r;
  }
  ++r.steps;

  std::printf("[red:module_stomp] step 2: locate client.dll / primary module\n");
  if (g->modules.empty()) {
    r.detail = "target module list empty";
    return r;
  }
  sim::Module* target = nullptr;
  for (auto& m : g->modules) {
    if (m.name.find("client") != std::string::npos ||
        m.name.find("game") != std::string::npos) {
      target = &m;
      break;
    }
  }
  if (target == nullptr) target = &g->modules.front();
  ++r.steps;

  std::printf("[red:module_stomp] step 3: stomp .text integrity hash in-place\n");
  target->text_hash = "stomped";
  target->headers_erased = true;
  if (target->text_hash != "stomped") {
    r.detail = "text_hash stomp failed";
    return r;
  }
  ++r.steps;

  std::printf("[red:module_stomp] step 4: verify integrity residual on known module\n");
  bool stomped = false;
  for (const auto& m : g->modules) {
    if (m.text_hash == "stomped" || m.headers_erased) stomped = true;
  }
  if (!stomped) {
    r.detail = "post-condition: stomped module not observed";
    return r;
  }
  ++r.steps;

  r.achieved = true;
  r.detail = "module_stomp red: " + target->name + " text_hash=stomped headers_erased=1";
  w.note(r.detail);
  std::printf("[red:module_stomp] achieved in %d steps\n", r.steps);
  return r;
}

}  // namespace examples::module_stomp
