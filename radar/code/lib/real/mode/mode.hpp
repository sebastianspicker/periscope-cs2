// mode.hpp — Runtime execution mode and tier availability.
//
// Every strategy has two paths: REAL (live CS2 via OS/hardware backends)
// and SIM (sim::World, deterministic, always works). Hybrid mode tries
// REAL first and falls back to SIM.
//
// This module also tracks which T0-T4 backends are available on the
// current system, so strategy pairs can dispatch to the correct path.
//
// Probes are self-contained (no link dependency on optional LR_ENABLE_REAL_KERNEL
// / DMA / full VMX targets). Absent hardware or privilege yields honest false.

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace real::mode {

/// Execution mode for a strategy pair.
enum class Mode : std::uint8_t {
  Real = 0,   ///< Operate on live CS2 via OS/hardware backends.
  Sim = 1,    ///< Operate on sim::World (deterministic, no external deps).
  Hybrid = 2, ///< Try Real first; fall back to Sim seamlessly.
};

/// Anti-cheat evasion / memory access tiers.
/// Maps to the tiered strategy curriculum.
enum class Tier : std::uint8_t {
  T0_UsermodeRpm = 0,   ///< OpenProcess + ReadProcessMemory
  T1_Syscall = 1,       ///< Direct syscalls bypassing usermode hooks
  T2_BYOVD = 2,         ///< Bring Your Own Vulnerable Driver (kernel)
  T3_Hypervisor = 3,    ///< Personal hypervisor (VT-x / SVM)
  T4_DMA = 4,           ///< PCIe DMA / FPGA / Thunderbolt
  Count = 5,
};

/// Human-readable name for a mode.
inline const char* mode_name(Mode m) {
  switch (m) {
    case Mode::Real:    return "real";
    case Mode::Sim:     return "sim";
    case Mode::Hybrid:  return "hybrid";
  }
  return "unknown";
}

/// Parse a mode name string ("real"/"sim"/"hybrid", case-insensitive).
/// Returns false if the string is not a known mode.
bool parse_mode(const char* text, Mode& out);

/// Human-readable name for a tier.
inline const char* tier_name(Tier t) {
  switch (t) {
    case Tier::T0_UsermodeRpm: return "T0 (usermode RPM)";
    case Tier::T1_Syscall:     return "T1 (direct syscall)";
    case Tier::T2_BYOVD:       return "T2 (BYOVD kernel)";
    case Tier::T3_Hypervisor:  return "T3 (personal HV)";
    case Tier::T4_DMA:         return "T4 (PCIe DMA)";
    default:                   return "unknown tier";
  }
}

inline const char* tier_short_name(Tier t) {
  switch (t) {
    case Tier::T0_UsermodeRpm: return "T0";
    case Tier::T1_Syscall:     return "T1";
    case Tier::T2_BYOVD:       return "T2";
    case Tier::T3_Hypervisor:  return "T3";
    case Tier::T4_DMA:         return "T4";
    default:                   return "?";
  }
}

/// Default CS2 process name (obfuscated at compile time on Windows).
const char* default_cs2_name();

/// Global runtime configuration.
struct RuntimeConfig {
  Mode mode = Mode::Hybrid;
  bool verbose = true;
  const char* cs2_process_name = nullptr;
  const char* driver_device_path = nullptr;
  int max_attach_retries = 3;

  RuntimeConfig() {
    cs2_process_name = default_cs2_name();
    load_from_env();
  }

  /// Load / re-load knobs from the process environment:
  ///   LR_MODE=real|sim|hybrid
  ///   LR_VERBOSE=1|true|0|false
  ///   LR_DRV_PATH=<device path, e.g. \\\\.\\gdrv>
  void load_from_env();

  bool may_use_real() const {
    return mode == Mode::Real || mode == Mode::Hybrid;
  }
  bool may_use_sim() const {
    return mode == Mode::Sim || mode == Mode::Hybrid;
  }
};

/// Global runtime config singleton.
inline RuntimeConfig& config() {
  static RuntimeConfig cfg;
  return cfg;
}

/// Set the runtime mode programmatically (does not touch the environment).
/// When config().verbose is true, prints a one-line status message.
void set_mode(Mode m);

// ═══════════════════════════════════════════════════════════════════════
// CS2 availability
// ═══════════════════════════════════════════════════════════════════════

/// Check whether a real CS2 process can be found on this system.
/// Returns true if CS2 is running (process enumeration hit).
bool cs2_available();

// ═══════════════════════════════════════════════════════════════════════
// Tier availability
// ═══════════════════════════════════════════════════════════════════════

/// Structured result of a single tier probe (yes/no + human reason).
struct TierProbe {
  Tier tier = Tier::T0_UsermodeRpm;
  bool available = false;
  std::string reason;
};

/// Check whether a specific tier's real backend is available.
/// T0: CS2 process + basic process access (QUERY_LIMITED / /proc).
/// T1: direct syscall capability (x64 + ntdll/syscall path present).
/// T2: loaded vulnerable driver device, physmem node, or /dev/mem.
/// T3: VMX or SVM reported by CPUID.
/// T4: DMA-class PCIe / Thunderbolt / USB4 / FPGA device signals.
bool tier_available(Tier t);

/// Full probe for one tier with a diagnostic reason string.
TierProbe probe_tier(Tier t);

/// Probe every curriculum tier (T0..T4) once.
std::vector<TierProbe> probe_all_tiers();

/// Check all tiers and return a bitmask of available ones.
/// Bit 0 = T0, Bit 1 = T1, ..., Bit 4 = T4.
std::uint8_t available_tiers_mask();

/// Highest available tier index in [0, 4], or -1 if none are available.
int highest_available_tier();

// ═══════════════════════════════════════════════════════════════════════
// Description / diagnostic
// ═══════════════════════════════════════════════════════════════════════

/// Describe the current runtime configuration in a string.
/// No side effects (no printf). Works without CS2 or privileged hardware.
std::string describe_mode();

/// Describe tier availability in a string (short names T0..T4).
/// No side effects (no printf).
std::string describe_tier_availability();

}  // namespace real::mode
