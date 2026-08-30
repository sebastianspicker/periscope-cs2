// netlink_audit.hpp — Audit / netlink scar surface probes.
//
// Kernel module IOCTLs and credential changes often produce audit records.
// This helper checks whether auditd/netlink audit is alive.

#pragma once

#include "real/error.hpp"

#include <cstdint>
#include <string>

namespace real::linux::audit {

struct AuditStatus {
    bool audit_enabled = false;     // /proc/sys/kernel/audit_enabled
    bool audit_syscalls = false;    // rough: audit_backlog_limit present
    int audit_pid = 0;              // /proc/sys/kernel/audit_pid
    std::string notes;
};

Result<AuditStatus> probe() noexcept;

/// Pure: parse "0"/"1" sysctl text.
bool parse_sysctl_bool(const std::string& text, bool& out) noexcept;

} // namespace real::linux::audit
