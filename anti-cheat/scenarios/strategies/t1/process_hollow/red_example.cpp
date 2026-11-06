#include "red_example.hpp"

#include <cstdio>

namespace examples::process_hollow {

RedResult apply(sim::World& w) {
  RedResult r{};
  const auto game = w.game_pid();
  const auto* target = w.proc(game);
  std::printf("[red:process_hollow] step 1: validate game arena\n");
  if (game == 0 || target == nullptr) {
    r.detail = "precondition failed: game process missing";
    return r;
  }
  ++r.steps;

  std::printf("[red:process_hollow] step 2: spawn hollow façade process\n");
  const auto actor = w.spawn("svchost.exe");
  auto* p = w.proc(actor);
  if (actor == 0 || p == nullptr) {
    r.detail = "hollow host spawn failed";
    return r;
  }
  ++r.steps;

  std::printf("[red:process_hollow] step 3: hollow image + map region scars\n");
  p->hollowed = true;
  p->original_image = "C:\\Windows\\System32\\svchost.exe";
  p->looks_reputable = true;
  p->manual_mapped_region = true;
  w.hollowed_pid = actor;
  w.hollowed_original_image = p->original_image;
  if (!p->hollowed || !p->looks_reputable || !p->manual_mapped_region) {
    r.detail = "hollow scars failed to plant";
    return r;
  }
  ++r.steps;

  std::printf("[red:process_hollow] step 4: OpenProcess VM_READ from hollowed host\n");
  if (!w.open_process(actor, game, sim::AccessMask::VmRead, false)) {
    r.detail = "VM_READ open from hollowed host failed";
    return r;
  }
  ++r.steps;

  std::printf("[red:process_hollow] step 5: read entities through hollow façade\n");
  const auto rr = w.read_mem(actor, game, target->base, 4, true);
  if (rr.status != ac::Status::Ok) {
    r.detail = "entity read failed";
    return r;
  }
  ++r.steps;

  p = w.proc(actor);
  bool vm_read = false;
  for (const auto& h : w.handles_to(game, true)) {
    if (h.owner_pid == actor && sim::has(h.access, sim::AccessMask::VmRead)) {
      vm_read = true;
      break;
    }
  }
  if (!(p && p->hollowed && p->looks_reputable && p->manual_mapped_region &&
        vm_read)) {
    r.detail = "post-condition: hollow + reputable + mapped + vm_read required";
    return r;
  }

  r.achieved = true;
  r.detail = "process hollow of svchost façade with VM_READ attach";
  w.note(r.detail);
  std::printf("[red:process_hollow] achieved in %d steps\n", r.steps);
  return r;
}

}  // namespace examples::process_hollow
