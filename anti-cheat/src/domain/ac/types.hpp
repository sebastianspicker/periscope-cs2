// types.hpp — core AC lab types: Tier, Status, EntitySnapshot, ReadRequest/Result.
// Shared by red backends and blue telemetry; Simulated educational enums/structs.

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ac {

// Delivery depth T0–T3 (T4 residual lives beside DMA paths).
enum class Tier : std::uint8_t {
  T0_UsermodeRpm = 0,
  T1_SyscallSoft = 1,
  T2_KernelByovd = 2,
  T3_Hypervisor = 3,
};

// Result codes for attach/read and lab-only paths.
enum class Status : std::uint8_t {
  Ok = 0,
  NotImplemented,
  Denied,
  Unavailable,
  InvalidArgument,
  LabOnly,
};

// Vec2: simple 2D position for UI and radar coordinates.
struct Vec2 {
  float x = 0;
  float y = 0;
};

// Vec3: simple 3D position for EntitySnapshot.
struct Vec3 {
  float x = 0;
  float y = 0;
  float z = 0;
};

// EntitySnapshot: one entity row red radar / blue scorers share.
struct EntitySnapshot {
  std::uint32_t id = 0;
  Vec3 origin{};
  Vec3 eye_angles{};
  std::uint8_t team = 0;
  std::int32_t health = -1;
  bool alive = false;
  bool dormant = false;
  bool is_local_player = false;
  bool is_bomb_carrier = false;
  char name[32]{};
  std::int32_t weapon_id = 0;

  constexpr EntitySnapshot() = default;
  constexpr EntitySnapshot(std::uint32_t entity_id, Vec3 position,
                           std::uint8_t entity_team, bool is_alive)
      : id(entity_id), origin(position), team(entity_team), alive(is_alive) {}
};

// ReadRequest: address+size for IMemoryBackend::read.
struct ReadRequest {
  std::uint64_t address = 0;
  std::size_t size = 0;
};

// ReadResult: Status + bytes from a lab read.
struct ReadResult {
  Status status = Status::NotImplemented;
  std::vector<std::uint8_t> bytes;
};

// to_string: Human-readable name for Tier/Status enums (logging/tests).
std::string_view to_string(Tier t);
// to_string: Human-readable name for Tier/Status enums (logging/tests).
std::string_view to_string(Status s);

/// How red acquired the handle (or avoided acquiring one) to the game process.
/// Handle visibility in the system handle table is the single point of failure
/// for memory read approaches 1-4. No amount of usermode masking hides a handle.
enum class HandleAcquisitionModel : std::uint8_t {
  None,              // No handle acquisition
  DirectOpenProcess, // OpenProcess(PROCESS_VM_READ) - instant detection via handle table
  NtOpenProcess,     // NtOpenProcess - handle still visible, no IAT entry
  DirectSyscall,     // syscall instruction - handle still visible despite call path hiding
  HandleDuplicate,   // DuplicateHandle from donor - reduced rights
  KernelDriver,      // MmCopyVirtualMemory via kernel driver
  HijackProxy,       // No handle from radar PID - reads delegated to donor that already holds handle
  DmaPhysical,       // Physical memory read via DMA - no handle at all
};

/// Which mechanism red uses to read game memory.
/// Maps to lab tiers but is independent (e.g., T0 can use HijackProxy).
enum class MemoryAcquisitionModel : std::uint8_t {
  DirectRpm,   // ReadProcessMemory / NtReadVirtualMemory
  Syscall,     // Direct syscall instruction
  KernelIoctl, // Kernel driver IOCTL interface
  HvHypercall, // Hypervisor hypercall
  DmaPhysical, // DMA physical memory read
  HijackProxy, // No handle from radar PID - all reads via donor callback
};

/// Legitimate software profile that red disguises itself as.
/// VAC must tolerate these tools (RTSS, Discord, Steam, etc.).
enum class DisguiseProfile : std::uint8_t {
  None,
  SteamOverlay,     // GameOverlayRenderer64.dll pattern - first-party
  DiscordOverlay,   // Discord overlay - third-party gaming accessory
  RivaTuner,        // RTSS.exe - reads game via NtReadVirtualMemory legitimately
  ObsStudio,        // graphics-hook64.dll game capture for streaming
  NvidiaShadowplay, // nvsphelper64.exe game read for recording
  GenericMonitor,   // Generic GPU monitoring tool (Afterburner, etc.)
};

/// Defense-in-depth layer. Red strategies specify which layers they use.
/// Blue scoring evaluates which layers are penetrated.
enum class DefenseLayer : std::uint8_t {
  ProcessIsolation,        // External radar, no game injection
  ProxyMemoryAccess,       // Hijack/proxy reader - no direct handle
  HardwareMonitorDisguise, // GPU monitoring tool mimicry
  ForensicTraceRemoval,    // Post-execution cleanup
  PeLegitimacy,            // Binary structure integrity
  SystemNormalization,     // Handle count, WER suppression, power profile
  BehavioralJitter,        // Frame drops, fuzz, latency - VACnet evasion
};

/// Independent observation views that anti-cheat uses to detect cheats.
/// Blue requires anomalies in 2+ views for actionable detection.
enum class ObservationView : std::uint8_t {
  HandleTable,   // System-wide handle enumeration (VAC external)
  ModuleList,    // VAS walk - loaded modules in game process
  MemoryPattern, // RPM volume/target/pattern analysis
  InProcess,     // Thread start, VMT, exceptions inside game (CS2 in-process)
  Behavioral,    // VACnet gameplay pattern analysis
  PostExecution, // Forensic artifacts on disk
};

/// Verdict from a single observation view.
struct ObservationVerdict {
  ObservationView view;
  bool anomaly_detected = false;
  double confidence = 0.0; // 0.0-1.0
  std::string detail;
};

/// Handle access-rights used by sim handle-table strategies.
/// Bit layout mirrors sim::AccessMask so it can drive sim::has().
enum class HandleAccess : std::uint32_t {
  None = 0,
  Query = 1u << 0,
  Read = 1u << 1,   // PROCESS_VM_READ equivalent
  Write = 1u << 2,
  All = 0xFFFFFFFFu,
};

// to_string: Human-readable names for acquisition, disguise, defense, and observation enums.
std::string_view to_string(HandleAcquisitionModel model);
std::string_view to_string(MemoryAcquisitionModel model);
std::string_view to_string(DisguiseProfile profile);
std::string_view to_string(DefenseLayer layer);
std::string_view to_string(ObservationView view);

}  // namespace ac
