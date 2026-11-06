// false_positive_policy.cpp — FP policy knobs so legit tools are not auto-banned in lab.
// Teaches multi-reason thresholds instead of single-handle ragebans.

#include "t0_blue/false_positive_policy.hpp"

namespace t0_blue {

// FalsePositivePolicy::allow_process_name: FP policy: name is allowlisted (legit software).
void FalsePositivePolicy::allow_process_name(std::string name) {
  names_.insert(std::move(name));
}

// FalsePositivePolicy::allow_signer: FP policy: signer subject is allowlisted.
void FalsePositivePolicy::allow_signer(std::string signer) {
  signers_.insert(std::move(signer));
}

// FalsePositivePolicy::is_signer_allowlisted: True if subject's signer is trusted.
bool FalsePositivePolicy::is_signer_allowlisted(
    const sim::Process& p) const noexcept {
  return signers_enabled && !p.signer.empty() && signers_.count(p.signer) > 0;
}

// FalsePositivePolicy::is_allowlisted: True if subject matches FP allowlist.
bool FalsePositivePolicy::is_allowlisted(const sim::Process& p) const noexcept {
  return names_.count(p.name) > 0 || is_signer_allowlisted(p);
}

// FalsePositivePolicy::should_alert: True if finding is strong enough to alert after FP filter.
bool FalsePositivePolicy::should_alert(const sim::Handle& h,
                                       const sim::World& w) const noexcept {
  return evaluate_edge(h, w).alert;
}

// FalsePositivePolicy::evaluate_edge: Apply allowlist to a handle-graph edge before alert.
FpDecision FalsePositivePolicy::evaluate_edge(const sim::Handle& h,
                                              const sim::World& w) const {
  FpDecision d;
  if (!sim::has(h.access, sim::AccessMask::VmRead)) {
    d.alert = false;
    d.reason = "not_vm_read";
    return d;
  }
  const auto* p = w.proc(h.owner_pid);
  if (!p) {
    d.alert = true;
    d.reason = "unknown_owner";
    return d;
  }
  if (p->is_game || p->is_ac) {
    d.alert = false;
    d.reason = "self_or_game";
    return d;
  }

  // Name allowlist alone is a weak, spoofable signal; track it separately from
  // independent reputation so a signer-allowlisted process can still suppress.
  d.name_allowlisted = names_.count(p->name) > 0;
  // Independent reputation: looks_reputable OR signer-allowlisted, and NOT
  // merely a classic FP name (masquerade war: names are spoofable, signers are not).
  d.reputation_safe =
      (p->looks_reputable || is_signer_allowlisted(*p)) && !d.name_allowlisted;

  if (d.reputation_safe) {
    // Still alert if handle is hidden / brief_reopen (evasion scar).
    if (h.hidden_during_enum || h.brief_reopen) {
      d.alert = true;
      d.reason = "reputable_but_enum_evasion";
      return d;
    }
    d.alert = false;
    d.reason = "independent_reputation";
    return d;
  }

  // Name allowlist alone is NOT enough (masquerade war).
  if (d.name_allowlisted) {
    d.alert = true;
    d.reason = "name_allowlist_insufficient_with_vm_read";
    return d;
  }

  d.alert = true;
  d.reason = "foreign_vm_read";
  return d;
}

}  // namespace t0_blue
