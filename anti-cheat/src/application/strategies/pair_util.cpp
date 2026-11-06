#include "strategies/pair_util.hpp"

#include <cstdio>
#include <algorithm>

namespace strategies {

std::uint32_t find_game(sim::World& w) { return w.game_pid(); }

bool any_vm_read(const sim::World& w, std::uint32_t game) {
  for (const auto& h : w.handles_to(game)) {
    if (!sim::has(h.access, sim::AccessMask::VmRead)) continue;
    const auto* p = w.proc(h.owner_pid);
    if (p && !p->is_game && !p->is_ac) {
      return true;
    }
  }
  return false;
}

int entity_count_via_handle(sim::World& w, std::uint32_t reader,
                            std::uint32_t game) {
  auto* g = w.proc(game);
  if (!g) {
    return -1;
  }
  auto r = w.read_mem(reader, game, g->base, 4, true);
  if (r.status != ac::Status::Ok || r.bytes.size() != sizeof(std::uint32_t)) {
    return -1;
  }
  std::uint32_t c = 0;
  std::copy_n(r.bytes.begin(), sizeof(c), reinterpret_cast<std::uint8_t*>(&c));
  return static_cast<int>(c);
}

int count_foreign_vm_read_owners(const sim::World& w, std::uint32_t game) {
  int n = 0;
  for (const auto& h : w.handles_to(game, true)) {
    if (!sim::has(h.access, sim::AccessMask::VmRead)) continue;
    const auto* p = w.proc(h.owner_pid);
    if (p && !p->is_game && !p->is_ac) ++n;
  }
  return n;
}

bool open_and_read_entity_count(sim::World& w, std::uint32_t reader,
                                std::uint32_t game, int* out_count) {
  if (out_count) *out_count = -1;
  auto* g = w.proc(game);
  if (!g || !g->is_game) return false;
  if (!w.open_process(reader, game, sim::AccessMask::VmRead, false)) {
    return false;
  }
  const int count = entity_count_via_handle(w, reader, game);
  if (out_count) *out_count = count;
  return count >= 0;
}

StrategyResult make_pair_result(bool red_achieved, bool blue_detected,
                                bool blue_mitigated, std::string summary) {
  StrategyResult r;
  r.red_achieved = red_achieved;
  r.blue_detected = blue_detected;
  r.blue_mitigated = blue_mitigated;
  r.summary = std::move(summary);
  return r;
}

StrategyResult assemble_pair_result(const RedOutcome& red,
                                    const BlueOutcome& blue,
                                    sim::Narrator* n) {
  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated || (blue.detected && blue.signals >= 3);
  r.summary = red.detail;
  if (!blue.detail.empty()) {
    r.summary += " | " + blue.detail;
  } else {
    r.summary += " | blue signals=" + std::to_string(blue.signals) +
                 " risk=" + std::to_string(blue.risk);
  }
  if (n) {
    n->result(r.blue_detected || r.blue_mitigated || !r.red_achieved, r.summary);
  }
  return r;
}

bool pair_pass(const StrategyResult& r) {
  return r.blue_detected || r.blue_mitigated || !r.red_achieved;
}

std::string describe_pair_result(const StrategyResult& r) {
  std::string s = "red_achieved=";
  s += r.red_achieved ? "1" : "0";
  s += " blue_detected=";
  s += r.blue_detected ? "1" : "0";
  s += " blue_mitigated=";
  s += r.blue_mitigated ? "1" : "0";
  s += " pass=";
  s += pair_pass(r) ? "1" : "0";
  s += " | ";
  s += r.summary;
  return s;
}

}  // namespace strategies
