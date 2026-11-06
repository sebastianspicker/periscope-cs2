#include "blue_example.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

namespace examples::parent_lineage {

namespace {
bool is_trusted_shell(const std::string& name) {
  return name == "explorer.exe" || name == "Discord.exe" ||
         name.find("discord") != std::string::npos ||
         name == "steam.exe" || name == "cmd.exe";
}
bool looks_suspicious_child(const std::string& name) {
  return name.find("radar") != std::string::npos ||
         name.find("cheat") != std::string::npos ||
         name.find("parent_lineage") != std::string::npos ||
         name.find("lab-actor") != std::string::npos ||
         name.find("reader") != std::string::npos;
}
}  // namespace

BlueResult detect(sim::World& w) {
  BlueResult r{};
  const auto game = w.game_pid();
  int lineage_hits = 0;
  int vm_read_under_shell = 0;
  int suspicious_under_shell = 0;

  std::printf("[blue:parent_lineage] sensor 1: parent reputation vs lineage anomaly\n");
  for (const auto& p : w.list_processes(false)) {
    if (p.is_game || p.is_ac) continue;
    const auto* parent = w.proc(p.parent_pid);
    if (parent == nullptr) continue;
    if (p.parent_pid == game && p.looks_reputable) {
      ++lineage_hits;
      r.reasons.emplace_back(
          "reputable process has implausible game parent lineage");
    }
    if (is_trusted_shell(parent->name) && looks_suspicious_child(p.name)) {
      ++suspicious_under_shell;
      ++lineage_hits;
      r.reasons.emplace_back("suspicious child under trusted shell parent: " +
                             p.name);
    }
  }

  std::printf("[blue:parent_lineage] sensor 2: VM_READ under shell parent\n");
  for (const auto& h : w.handles_to(game, true)) {
    if (!sim::has(h.access, sim::AccessMask::VmRead)) continue;
    const auto* owner = w.proc(h.owner_pid);
    if (owner == nullptr || owner->is_game || owner->is_ac) continue;
    const auto* parent = w.proc(owner->parent_pid);
    if (parent != nullptr &&
        (is_trusted_shell(parent->name) || parent->looks_reputable ||
         owner->looks_reputable)) {
      ++vm_read_under_shell;
    }
  }
  if (vm_read_under_shell > 0) {
    r.reasons.emplace_back(
        "foreign VM_READ whose owner has trusted/reputable shell parent");
  }

  std::printf("[blue:parent_lineage] sensor 3: handle graph join\n");
  bool foreign_handle = false;
  for (const auto& h : w.handles_to(game, true)) {
    if (!sim::has(h.access, sim::AccessMask::VmRead)) continue;
    const auto* owner = w.proc(h.owner_pid);
    if (owner != nullptr && !owner->is_game && !owner->is_ac) {
      foreign_handle = true;
      break;
    }
  }
  if (foreign_handle && lineage_hits > 0) {
    r.reasons.emplace_back("handle graph joins with lineage anomaly");
  }

  r.signals = static_cast<int>(r.reasons.size());
  r.risk = std::min(1.0, r.signals * 0.24);
  r.detected = r.signals >= 2;
  r.mitigated = vm_read_under_shell > 0 ||
                (foreign_handle && lineage_hits > 0);
  if (r.risk >= 0.8) w.ranked_access_denied = true;
  r.detail = "lineage=" + std::to_string(lineage_hits) +
             " vm_under_shell=" + std::to_string(vm_read_under_shell) +
             " signals=" + std::to_string(r.signals);
  std::printf("[blue:parent_lineage] %s detected=%d mitigated=%d\n",
              r.detail.c_str(), r.detected, r.mitigated);
  return r;
}

}  // namespace examples::parent_lineage
