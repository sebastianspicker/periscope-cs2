// preflight.hpp — pre-attach system posture report (E61).
//
// Loading a kernel driver into a hardened Windows install fails in ways that
// are hard to read from a raw SCM error: Secure Boot with test signing off,
// HVCI, the vulnerable-driver blocklist, or simply not being admin. This
// header classifies the observed state into a verdict and a human-readable
// explanation. Probes are best-effort; an unknown probe is reported as
// unknown, never as a hard block.
//
// Pure classification is host-testable; `probe()` lives in preflight.cpp.

#pragma once

#include <string>

namespace real::kernel::preflight {

enum class Verdict {
  Ready,
  Warn,
  Blocked,
};

struct SystemStatus {
  bool is_admin = false;
  bool is_admin_known = false;
  bool secure_boot = false;
  bool secure_boot_known = false;
  bool test_signing = false;
  bool test_signing_known = false;
  bool hvci = false;
  bool hvci_known = false;
  bool blocklist = false;
  bool blocklist_known = false;
  bool driver_file = false;
  bool driver_file_known = false;
  bool driver_loaded = false;
};

/// Classify the state. A missing driver file or hardened boot is a warning
/// (the RPM path still works); missing admin is the only hard block.
inline Verdict classify(const SystemStatus& s) {
  if (s.is_admin_known && !s.is_admin) return Verdict::Blocked;
  if (s.driver_file_known && !s.driver_file) return Verdict::Warn;
  if (s.secure_boot && !s.test_signing) return Verdict::Warn;
  if (s.hvci || s.blocklist) return Verdict::Warn;
  return Verdict::Ready;
}

inline const char* verdict_name(Verdict verdict) {
  switch (verdict) {
    case Verdict::Ready: return "ready";
    case Verdict::Warn: return "warn";
    case Verdict::Blocked: return "blocked";
  }
  return "unknown";
}

inline const char* tri(bool value, bool known) {
  if (!known) return "unknown";
  return value ? "yes" : "no";
}

inline std::string describe(const SystemStatus& s) {
  std::string out;
  out += "verdict=";
  out += verdict_name(classify(s));
  out += " admin=";
  out += tri(s.is_admin, s.is_admin_known);
  out += " secure_boot=";
  out += tri(s.secure_boot, s.secure_boot_known);
  out += " test_signing=";
  out += tri(s.test_signing, s.test_signing_known);
  out += " hvci=";
  out += tri(s.hvci, s.hvci_known);
  out += " blocklist=";
  out += tri(s.blocklist, s.blocklist_known);
  out += " driver_file=";
  out += tri(s.driver_file, s.driver_file_known);
  out += " driver_loaded=";
  out += s.driver_loaded ? "yes" : "no";
  return out;
}

/// Best-effort probe. Non-Windows hosts return all-unknown.
SystemStatus probe(const char* driver_path);

}  // namespace real::kernel::preflight
