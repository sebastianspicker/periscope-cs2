// anti_debug.hpp — Linux debugger / tracer detection (BLUE sensors).
//
// Mirrors Windows PEB/NtQueryInformationProcess checks using:
//   /proc/self/status TracerPid
//   /proc/self/wchan, PtraceScope
//   prctl(PR_GET_DUMPABLE)
//   self-ptrace test
//
// SCAR (red): clearing TracerPid is not possible from usermode; red must
// avoid ptrace attachment scars instead.

#pragma once

#include "real/error.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace real::linux::anti_debug {

struct DebugStatus {
    bool tracer_present = false;
    uint32_t tracer_pid = 0;
    bool dumpable = true;
    bool ptrace_scope_restricted = false;
    int ptrace_scope = -1; // /proc/sys/kernel/yama/ptrace_scope
    bool self_ptrace_blocked = false;
    std::vector<std::string> signals; // human-readable hits
};

/// Full self-inspection.
Result<DebugStatus> inspect_self() noexcept;

/// Read TracerPid from a status file blob (pure, unit-tested).
bool parse_tracer_pid(const std::string& status_text, uint32_t& out_pid) noexcept;

/// True if TracerPid != 0.
Result<bool> is_traced(uint32_t pid = 0 /* 0 = self */) noexcept;

/// Yama ptrace_scope (0..3). Error if sysctl missing.
Result<int> yama_ptrace_scope() noexcept;

/// Attempt PTRACE_TRACEME on self; returns true if a tracer already holds us.
Result<bool> self_ptrace_detected() noexcept;

} // namespace real::linux::anti_debug
