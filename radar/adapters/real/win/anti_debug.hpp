#pragma once

namespace real::win {

/// Clear PEB debug flags (BeingDebugged, NtGlobalFlag), heap force flags,
/// hide the current thread from debuggers, and clear hardware breakpoints.
///
/// LIMITATION: User-mode PEB writes do not change the kernel's authoritative
/// view. Kernel AC can still detect debugger attachment via the process
/// DebugPort / DebugObject.
void clear_debug_flags() noexcept;

/// Check if a debugger is attached via NtQueryInformationProcess classes
/// (DebugPort, DebugObjectHandle, DebugFlags), PEB BeingDebugged, hardware
/// breakpoints, and RDTSC single-step anomaly.
/// Returns true if a debugger is detected.
bool check_debugger_ntqsi() noexcept;

/// Broader check: NtQIP suite + IsDebuggerPresent + CheckRemoteDebuggerPresent.
bool check_debugger_present() noexcept;

}  // namespace real::win
