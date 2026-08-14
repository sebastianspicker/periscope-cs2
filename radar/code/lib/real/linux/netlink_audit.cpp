// netlink_audit.cpp — audit subsystem probes.

#include "real/linux/netlink_audit.hpp"
#include "real/platform.hpp"

#include <fstream>
#include <string>

namespace real::linux::audit {

bool parse_sysctl_bool(const std::string& text, bool& out) noexcept {
    if (text.empty()) return false;
    // trim
    size_t i = 0;
    while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) ++i;
    if (i >= text.size()) return false;
    if (text[i] == '1' || text[i] == 'y' || text[i] == 'Y') {
        out = true;
        return true;
    }
    if (text[i] == '0' || text[i] == 'n' || text[i] == 'N') {
        out = false;
        return true;
    }
    return false;
}

Result<AuditStatus> probe() noexcept {
    AuditStatus st;
#if LR_PLATFORM_LINUX
    {
        std::ifstream f("/proc/sys/kernel/audit_enabled");
        if (f) {
            std::string t;
            std::getline(f, t);
            parse_sysctl_bool(t, st.audit_enabled);
        } else {
            st.notes = "audit_enabled sysctl missing";
        }
    }
    {
        std::ifstream f("/proc/sys/kernel/audit_pid");
        if (f) f >> st.audit_pid;
    }
    {
        std::ifstream f("/proc/sys/kernel/audit_backlog_limit");
        st.audit_syscalls = static_cast<bool>(f);
    }
    return st;
#else
    st.notes = "audit probe requires Linux";
    return Result<AuditStatus>(st, "probe requires Linux");
#endif
}

} // namespace real::linux::audit
