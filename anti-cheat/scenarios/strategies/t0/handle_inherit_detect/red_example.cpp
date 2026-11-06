#include "red_example.hpp"

#include <cstdio>

namespace strategy::t0_handle_inherit_detect {

void Red::apply(sim::World& w) noexcept {
  std::printf("[red:handle_inherit_detect] step 1: validate game arena\n");
  const auto game = w.game_pid();
  const auto* g = w.proc(game);
  if (game == 0 || g == nullptr) {
    w.note("handle_inherit_detect: precondition failed — game missing");
    return;
  }

  std::printf("[red:handle_inherit_detect] step 2: spawn parent reader (opens game handle)\n");
  const auto parent = w.spawn("handle_inherit-parent.exe");
  if (parent == 0 || w.proc(parent) == nullptr) {
    w.note("handle_inherit_detect: parent spawn failed");
    return;
  }
  if (!w.open_process(parent, game, sim::AccessMask::VmRead, false)) {
    w.note("handle_inherit_detect: parent OpenProcess failed");
    return;
  }

  std::printf("[red:handle_inherit_detect] step 3: spawn child that inherits VM_READ\n");
  const auto child = w.spawn("cheat_child.exe");
  auto* cp = w.proc(child);
  if (child == 0 || cp == nullptr) {
    w.note("handle_inherit_detect: child spawn failed");
    return;
  }
  cp->parent_pid = parent;

  std::printf("[red:handle_inherit_detect] step 4: plant inherited/proxy handle on child\n");
  // Child receives handle without its own OpenProcess (inheritance / via_proxy scar)
  if (!w.open_process(child, game, sim::AccessMask::VmRead, false)) {
    w.note("handle_inherit_detect: child handle plant failed");
    return;
  }
  bool marked = false;
  for (auto& handle : w.handles) {
    if (handle.owner_pid == child && handle.target_pid == game &&
        sim::has(handle.access, sim::AccessMask::VmRead)) {
      handle.via_proxy = true;  // models inherited/proxy attribution
      marked = true;
    }
  }
  if (!marked) {
    w.note("handle_inherit_detect: via_proxy mark failed");
    return;
  }

  std::printf("[red:handle_inherit_detect] step 5: optional entity peek through child\n");
  (void)w.read_mem(child, game, g->base, 4, true);

  std::printf("[red:handle_inherit_detect] step 6: verify parent+child dual VM_READ\n");
  bool parent_h = false, child_h = false;
  for (const auto& h : w.handles_to(game, true)) {
    if (!sim::has(h.access, sim::AccessMask::VmRead)) continue;
    if (h.owner_pid == parent) parent_h = true;
    if (h.owner_pid == child && h.via_proxy) child_h = true;
  }
  if (!(parent_h && child_h && cp->parent_pid == parent)) {
    w.note("handle_inherit_detect: post-condition failed");
    return;
  }

  w.note("handle_inherit_detect: child PID " + std::to_string(child) +
         " inherits proxy VM_READ; parent also holds handle");
  std::printf("[red:handle_inherit_detect] achieved: parent=%u child=%u chain=1\n", parent,
              child);
}

}  // namespace strategy::t0_handle_inherit_detect
