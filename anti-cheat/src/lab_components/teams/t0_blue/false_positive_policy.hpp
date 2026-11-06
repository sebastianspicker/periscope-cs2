#pragma once

// Real AC pain: overlay tools, recorders, RGB software open process handles.
// Strong policy: name allowlist alone is not enough when VM_READ + weak rep.

#include "sim/world.hpp"

#include <string>
#include <unordered_set>

namespace t0_blue {

// Lab type `FpDecision` used by this educational unit.
struct FpDecision {
  bool alert = true;
  bool name_allowlisted = false;
  bool reputation_safe = false;
  std::string reason;
};

// Lab type `FalsePositivePolicy` used by this educational unit.
class FalsePositivePolicy {
 public:
  void allow_process_name(std::string name);
  void allow_signer(std::string signer);
  bool is_signer_allowlisted(const sim::Process& p) const noexcept;

  bool is_allowlisted(const sim::Process& p) const noexcept;

  /// Classic soft allowlist: suppress alert if name matches.
  bool should_alert(const sim::Handle& h, const sim::World& w) const noexcept;

  /// Strong policy: name allowlist is not independent trust; require more.
  FpDecision evaluate_edge(const sim::Handle& h, const sim::World& w) const;

  // Toggle signer-based allowlist independently of the name allowlist.
  bool signers_enabled{true};

 private:
  std::unordered_set<std::string> names_;
  std::unordered_set<std::string> signers_;
};

}  // namespace t0_blue
