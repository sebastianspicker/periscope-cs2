#pragma once
// world_types.hpp — Lab residual types for sim::World (split from world.hpp).


// Simulated OS + game world shared by red and blue code.

#include "ac/types.hpp"
#include "cs2/diagnostics.hpp"
#include "sim/handle_table.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace sim {


enum class AccessMask : std::uint32_t {
  None = 0,
  Query = 1u << 0,
  VmRead = 1u << 1,
  VmWrite = 1u << 2,
  VmOperation = 1u << 3,
};

inline AccessMask operator|(AccessMask a, AccessMask b) {
  return static_cast<AccessMask>(static_cast<std::uint32_t>(a) |
                                 static_cast<std::uint32_t>(b));
}
inline bool has(AccessMask a, AccessMask f) {
  return (static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(f)) != 0;
}
inline bool has(AccessMask a, ac::HandleAccess f) {
  return has(a, static_cast<AccessMask>(static_cast<std::uint32_t>(f)));
}

/// VT-d / IOMMU remapping state model (uefi_dma_bypass strategy).
enum class IommuState : std::uint8_t {
  Disabled,
  Enabled,
  Bypassed,  // ATS/ACS bypass confirmed
};

// Lab type `TpmEventLogEntry` used by this educational unit (uefi_txt_measured_boot).
struct TpmEventLogEntry {
  std::uint32_t pcr_index = 0;
  std::uint32_t event_type = 0;  // TCG EV_* type
  std::string digest;            // hex digest recorded in the log
  std::string description;       // human-readable event description
};

// Lab type `Module` used by this educational unit.
struct Module {
  std::string name;
  std::uint64_t base = 0;
  std::size_t size = 0;
  bool linked_in_peb = true;
  bool headers_erased = false;
  std::string text_hash = "clean";
  bool iat_hooked = false;
  bool eat_hooked = false;
  bool present_hooked = false;  // DXGI/Present
};

// Lab type `Process` used by this educational unit.
struct Process {
  std::uint32_t pid = 0;
  std::string name;
  bool is_game = false;
  std::uint8_t team = 0;
  bool is_ac = false;
  bool hidden_from_weak_enum = false;
  std::vector<std::uint8_t> memory;
  std::uint64_t base = 0x10000000ull;
  std::vector<Module> modules;
  bool has_foreign_thread = false;
  bool manual_mapped_region = false;
  std::uint32_t parent_pid = 0;       // launch lineage
  bool reader_active = false;        // in-match-only red
  bool looks_reputable = false;      // signed/known-good reputation flag
  std::string signer;                // verified code-signing subject (signer allowlist)
  bool hollowed = false;             // process hollowing scar
  std::string original_image;        // path before hollow
  bool thread_hijacked = false;      // APC / hijack on this process
  std::uint64_t cr3 = 0;             // lab CR3 token for stealth target
  // Process-identity telemetry used exclusively by simulated evasion lessons.
  std::string window_class;
  std::string command_line;
  std::size_t working_set_kb = 0;
  bool timing_jittered = false;
  bool peb_identity_matched = false;
};

// Lab type `Handle` used by this educational unit.
struct Handle {
  std::uint32_t owner_pid = 0;
  std::uint32_t target_pid = 0;
  AccessMask access = AccessMask::None;
  bool via_syscall_path = false;
  bool brief_reopen = false;  // open only for a tick
  bool hidden_during_enum = false;  // red closes/hides while AC samples
  bool via_proxy = false;  // owned by proxy/legit process for a cheat consumer
};

// Lab type `SharedSection` used by this educational unit.
struct SharedSection {
  std::string name;
  std::uint32_t creator_pid = 0;
  std::uint32_t consumer_pid = 0;  // game or mapper
  bool carries_entity_bytes = false;
};

// Lab type `Driver` used by this educational unit.
struct Driver {
  std::string name;
  std::string sha256;
  std::string signer;
  bool boot_start = false;
  bool byovd_known_bad = false;
  bool is_ac = false;
  bool is_bridge = false;
  bool provides_mem_rw = false;
  int load_order = 100;  // lower = earlier; AC typically mid
};

// Lab type `Device` used by this educational unit.
struct Device {
  std::string name;
  std::string owner_driver;
  bool mem_rw_ioctl = false;
};

// ── Band-4 overlay (createwindowinband via explorer injection) ──
enum class ZOrderBand : std::uint8_t {
  Normal,
  Topmost,
  Band4,  // ZWID_IMMERSIVE_NOTIFICATION — above fullscreen games
};

// Lab type `OverlayWindow` used by this educational unit.
struct OverlayWindow {
  std::uint32_t owner_pid = 0;
  std::string title;
  bool topmost = false;
  bool transparent = false;
  bool hijacks_swapchain = false;
  bool stream_proof = false;   // not visible to capture/OBS path
  bool band4_zorder = false;   // elevated Z-band (fullscreen-capable residual)
  ZOrderBand z_order = ZOrderBand::Normal;
  bool explorer_injected_for_band4 = false;
  std::uint32_t band4_explorer_pid = 0;

  // Steam overlay hook: GameOverlayRenderer64.dll pointer swap surface.
  // When true, red has overwritten Steam's Present/ResizeBuffers
  // trampoline pointers in the overlay DLL's .data section.
  bool steam_overlay_hijacked = false;
  // The PID of the GameOverlayRenderer64.dll loader process.
  std::uint32_t steam_overlay_owner_pid = 0;

  // Window hijack: D3D11 swap chain pointed at another process's HWND
  // (e.g., Discord overlay). When true, the presenter PID != HWND owner PID.
  bool window_hijacked = false;
  // The PID that owns the target HWND (e.g., Discord.exe).
  std::uint32_t hwnd_owner_pid = 0;
  // The PID doing the D3D11 present (the cheat).
  std::uint32_t presenter_pid = 0;
  // Whether the hijack is cross-process (detectable).
  bool cross_process_hijack = false;

  // Pinned swapchain: red only renders to the first stable swapchain
  // after N frames (typically 120), ignoring loading screen chains.
  bool swapchain_pinned = false;
  // Number of frames waited before pinning.
  int pin_wait_frames = 0;
};

// Lab type `InputEvent` used by this educational unit.
struct InputEvent {
  double t = 0;
  std::string source;  // "raw_hid" | "injected" | "serial_arduino" | "kmbox"
  float dx = 0, dy = 0;
};

// Lab type `HostTrust` used by this educational unit.
struct HostTrust {
  bool secure_boot = true;
  bool vbs = true;
  bool hvci = true;
  bool iommu_on = true;
  bool personal_hv_active = false;
  bool platform_hv_active = false;  // e.g. Hyper-V / VBS host
  bool dma_device_present = false;
  bool capture_card_present = false;  // T4 CV residual
  std::string hv_vendor;
  std::string platform_hv_vendor = "Microsoft Hv";
  double cpuid_latency_ns = 100;
  double baseline_latency_ns = 100;
  bool timing_spoofed = false;  // red multi-leaf inconsistency
  std::string hwid = "disk:AAA|mac:BBB|smbios:CCC";
  bool attestation_valid = true;
  bool attestation_pcr_ok = true;
  bool ept_hide_ac_pages = false;     // red hides AC from guest view
  bool unexpected_efi_entry = false;  // EFI boot entry scar
  std::string efi_entry_name;
  bool dual_boot_profile = false;     // alternate OS / PCR path
  std::string boot_pcr_profile = "known_good";
  bool elam_enabled = true;           // early launch anti-malware
  bool secure_launch = true;          // measured boot / secure launch
  bool ranked_requires_iommu = false; // T4 policy
  bool dse_enforced = true;           // driver signature enforcement
  bool test_signing = false;
  bool hvci_enabled = true;
  bool secure_kernel_view_dirty = false;  // dual-view: SK sees tamper
  bool guest_ac_view_clean = true;        // guest self-check lies under hide
  bool measured_launch_hidden = false;  // TXT/SecureLaunch hide scar (uefi_txt_measured_boot)
};

// Lab type `AimSample` used by this educational unit.
struct AimSample {
  float camera_yaw = 0;
  float camera_pitch = 0;
  float server_aim_yaw = 0;   // what hitreg uses (silent aim desync)
  float server_aim_pitch = 0;
  bool challenge_passed = true;  // server aim challenge-response
  // Info-advantage residual (legit radar / pre-aim without vision or audio).
  bool aim_on_hidden_target = false;
  bool has_vision_on_target = false;
  bool has_audio_on_target = false;
  // Triggerbot residual: fire only when crosshair already on enemy.
  bool crosshair_on_enemy = false;
  bool fired = false;
  float fire_latency_ms = 0.f;  // click delay after on-target (low = bot)
  // RCS residual: pitch correction tracks expected spray pattern.
  float recoil_expected_pitch = 0.f;
  float rcs_applied_pitch = 0.f;
  // Sound ESP: position known from audio/movement only (no LOS).
  bool position_from_sound_only = false;
};

// Lab type `ServiceEvent` used by this educational unit.
struct ServiceEvent {
  std::string name;
  std::string driver_image;
  bool kernel_driver = false;
};

// Lab type `Account` used by this educational unit.
struct Account {
  std::string id;
  std::string hwid;
  std::string payment_fp;
  std::string ip_class;
  int reports = 0;
};

// Lab type `NetFlow` used by this educational unit.
struct NetFlow {
  std::uint32_t pid = 0;
  std::string dest;  // host:port or domain
  bool looks_like_offset_c2 = false;
  bool looks_like_radar_saas = false;
};

// Shared scar surface: processes, handles, drivers, trust, inputs, net, accounts.
}  // namespace sim
