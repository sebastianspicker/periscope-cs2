// types.cpp — implements types.
// Simulation unit; scars live on sim::World.

#include "ac/types.hpp"

namespace ac {

// to_string: Human-readable name for Tier enum (logging/tests).
std::string_view to_string(Tier t) {
  switch (t) {
    case Tier::T0_UsermodeRpm:
      return "T0";
    case Tier::T1_SyscallSoft:
      return "T1";
    case Tier::T2_KernelByovd:
      return "T2";
    case Tier::T3_Hypervisor:
      return "T3";
  }
  return "T?";
}

// to_string: Human-readable name for Status enum (logging/tests).
std::string_view to_string(Status s) {
  switch (s) {
    case Status::Ok:
      return "ok";
    case Status::NotImplemented:
      return "not_implemented";
    case Status::Denied:
      return "denied";
    case Status::Unavailable:
      return "unavailable";
    case Status::InvalidArgument:
      return "invalid_argument";
    case Status::LabOnly:
      return "lab_only";
  }
  return "unknown";
}

// to_string: Human-readable name for HandleAcquisitionModel (logging/tests).
std::string_view to_string(HandleAcquisitionModel model) {
  switch (model) {
    case HandleAcquisitionModel::None:
      return "none";
    case HandleAcquisitionModel::DirectOpenProcess:
      return "direct_open_process";
    case HandleAcquisitionModel::NtOpenProcess:
      return "nt_open_process";
    case HandleAcquisitionModel::DirectSyscall:
      return "direct_syscall";
    case HandleAcquisitionModel::HandleDuplicate:
      return "handle_duplicate";
    case HandleAcquisitionModel::KernelDriver:
      return "kernel_driver";
    case HandleAcquisitionModel::HijackProxy:
      return "hijack_proxy";
    case HandleAcquisitionModel::DmaPhysical:
      return "dma_physical";
  }
  return "unknown_handle_acq";
}

// to_string: Human-readable name for MemoryAcquisitionModel (logging/tests).
std::string_view to_string(MemoryAcquisitionModel model) {
  switch (model) {
    case MemoryAcquisitionModel::DirectRpm:
      return "direct_rpm";
    case MemoryAcquisitionModel::Syscall:
      return "syscall";
    case MemoryAcquisitionModel::KernelIoctl:
      return "kernel_ioctl";
    case MemoryAcquisitionModel::HvHypercall:
      return "hv_hypercall";
    case MemoryAcquisitionModel::DmaPhysical:
      return "dma_physical";
    case MemoryAcquisitionModel::HijackProxy:
      return "hijack_proxy";
  }
  return "unknown_mem_acq";
}

// to_string: Human-readable name for DisguiseProfile (logging/tests).
std::string_view to_string(DisguiseProfile profile) {
  switch (profile) {
    case DisguiseProfile::None:
      return "none";
    case DisguiseProfile::SteamOverlay:
      return "steam_overlay";
    case DisguiseProfile::DiscordOverlay:
      return "discord_overlay";
    case DisguiseProfile::RivaTuner:
      return "riva_tuner";
    case DisguiseProfile::ObsStudio:
      return "obs_studio";
    case DisguiseProfile::NvidiaShadowplay:
      return "nvidia_shadowplay";
    case DisguiseProfile::GenericMonitor:
      return "generic_monitor";
  }
  return "unknown_disguise";
}

// to_string: Human-readable name for DefenseLayer (logging/tests).
std::string_view to_string(DefenseLayer layer) {
  switch (layer) {
    case DefenseLayer::ProcessIsolation:
      return "process_isolation";
    case DefenseLayer::ProxyMemoryAccess:
      return "proxy_memory_access";
    case DefenseLayer::HardwareMonitorDisguise:
      return "hardware_monitor_disguise";
    case DefenseLayer::ForensicTraceRemoval:
      return "forensic_trace_removal";
    case DefenseLayer::PeLegitimacy:
      return "pe_legitimacy";
    case DefenseLayer::SystemNormalization:
      return "system_normalization";
    case DefenseLayer::BehavioralJitter:
      return "behavioral_jitter";
  }
  return "unknown_defense";
}

// to_string: Human-readable name for ObservationView (logging/tests).
std::string_view to_string(ObservationView view) {
  switch (view) {
    case ObservationView::HandleTable:
      return "handle_table";
    case ObservationView::ModuleList:
      return "module_list";
    case ObservationView::MemoryPattern:
      return "memory_pattern";
    case ObservationView::InProcess:
      return "in_process";
    case ObservationView::Behavioral:
      return "behavioral";
    case ObservationView::PostExecution:
      return "post_execution";
  }
  return "unknown_observation";
}

}  // namespace ac
