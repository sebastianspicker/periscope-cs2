// diagnostic.hpp — Real CS2 diagnostic system reader.
//
// LESSON: CS2 has a built-in diagnostic system that collects data about
// the runtime environment: loaded modules, threads, VMT integrity,
// exception handlers, and counter-strafe analysis. This data is sent
// to Valve's servers and used for trust factor / VACnet decisions.
//
// This file reads the REAL diagnostic state from CS2 memory, giving
// us insight into what CS2 itself detects. Blue strategies use this
// data to understand what the game's built-in AC sees. Red strategies
// must evade these diagnostics to avoid trust factor penalties.
//
// Educational design:
//   REAL MODE:   Reads the real cs2.exe diagnostic structures.
//   SIM MODE:    Uses the cs2::DllVerificationState from sim.
//   Both modes teach the CS2 diagnostic system's detection surface.

#pragma once

#include "real/cs2/memory.hpp"
#include "real/cs2/offsets.hpp"
#include "real/error.hpp"
#include "real/mode/mode.hpp"
#include "real/platform.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace real::cs2 {

/// Diagnostic data read from the real CS2 process.
/// Mirrors the cs2::DllVerificationState from the simulation,
/// but reads actual values from the running game.
struct RealDiagnosticState {
  /// Module verification: list of loaded modules CS2 has verified.
  std::vector<std::string> verified_modules;
  int verified_module_count = 0;

  /// Thread capture: whether CS2 has captured thread info.
  bool thread_captured = false;
  int thread_count = 0;

  /// PE timestamps CS2 has collected.
  std::uint32_t pe_timestamp_client_dll = 0;
  std::uint32_t pe_timestamp_cs2_exe = 0;
  std::uint32_t pe_timestamp_kernel32 = 0;
  std::uint32_t pe_timestamp_ntdll = 0;

  /// ConVar integrity check state.
  bool convars_checked = false;
  int modified_convar_count = 0;

  /// VMT (virtual method table) integrity.
  bool vmt_collected = false;
  int hooked_vtables = 0;

  /// Exception handler status.
  bool exception_handler_active = false;
  int exception_count = 0;
  std::uint64_t last_exception_address = 0;

  /// Debugger detection.
  bool debugger_detected = false;

  /// CPUID-based VM detection.
  bool cpuid_vm_detected = false;

  /// Counter-strafe analysis results.
  int cs_perfect_frames = 0;
  int cs_total_actions = 0;
  double cs_average_tick_delta = 0.0;

  /// File integrity check results.
  bool file_integrity_checked = false;
  int modified_files = 0;

  /// Whether the client is allowed on secure servers.
  bool client_allowed_on_secure = true;

  std::string describe() const;
};

/// Read the CS2 diagnostic state from the real process.
/// REAL MODE:   Reads from cs2.exe memory.
/// SIM MODE:    Reads from sim::World.diagnostic_state.
Result<RealDiagnosticState> read_diagnostic_state(
    Cs2MemoryReader& reader, const Cs2Offsets& offsets);

/// Read just the module verification state.
Result<std::vector<std::string>> read_verified_modules(
    Cs2MemoryReader& reader, const Cs2Offsets& offsets);

/// Read thread capture data.
Result<int> read_thread_count(Cs2MemoryReader& reader,
                               const Cs2Offsets& offsets);

}  // namespace real::cs2
