// anti_debug.cpp — Linux tracer / debugger detection sensors.

#include "real/linux/anti_debug.hpp"
#include "real/platform.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#if LR_PLATFORM_LINUX
#  include <sys/prctl.h>
#  include <sys/ptrace.h>
#  include <unistd.h>
#endif

namespace real::linux::anti_debug {

bool parse_tracer_pid(const std::string& status_text, uint32_t& out_pid) noexcept {
    std::istringstream in(status_text);
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("TracerPid:", 0) == 0) {
            unsigned v = 0;
            if (std::sscanf(line.c_str(), "TracerPid:\t%u", &v) == 1 ||
                std::sscanf(line.c_str(), "TracerPid: %u", &v) == 1) {
                out_pid = v;
                return true;
            }
        }
    }
    return false;
}

Result<bool> is_traced(uint32_t pid) noexcept {
#if LR_PLATFORM_LINUX
    char path[64];
    if (pid == 0) {
        std::snprintf(path, sizeof(path), "/proc/self/status");
    } else {
        std::snprintf(path, sizeof(path), "/proc/%u/status", pid);
    }
    std::ifstream f(path);
    if (!f) return Result<bool>(false, "Cannot open status");
    std::ostringstream ss;
    ss << f.rdbuf();
    uint32_t tracer = 0;
    if (!parse_tracer_pid(ss.str(), tracer)) {
        return Result<bool>(false, "TracerPid missing");
    }
    return tracer != 0;
#else
    (void)pid;
    return Result<bool>(false, "is_traced requires Linux");
#endif
}

Result<int> yama_ptrace_scope() noexcept {
#if LR_PLATFORM_LINUX
    std::ifstream f("/proc/sys/kernel/yama/ptrace_scope");
    if (!f) return Result<int>(-1, "yama ptrace_scope unavailable");
    int scope = 0;
    f >> scope;
    return scope;
#else
    return Result<int>(-1, "yama requires Linux");
#endif
}

Result<bool> self_ptrace_detected() noexcept {
#if LR_PLATFORM_LINUX
    // If already traced, PTRACE_TRACEME fails with EPERM.
    if (::ptrace(PTRACE_TRACEME, 0, nullptr, nullptr) < 0) {
        return true;
    }
    // Undo — detach by raising and ignoring is messy; call TRACEME only once.
    // After successful TRACEME we are "tracing ourselves"; detach via:
    ::ptrace(PTRACE_DETACH, ::getpid(), nullptr, nullptr);
    return false;
#else
    return Result<bool>(false, "self_ptrace_detected requires Linux");
#endif
}

Result<DebugStatus> inspect_self() noexcept {
    DebugStatus st;
#if LR_PLATFORM_LINUX
    std::ifstream f("/proc/self/status");
    if (f) {
        std::ostringstream ss;
        ss << f.rdbuf();
        uint32_t tracer = 0;
        if (parse_tracer_pid(ss.str(), tracer)) {
            st.tracer_pid = tracer;
            st.tracer_present = tracer != 0;
            if (st.tracer_present) {
                st.signals.push_back("TracerPid=" + std::to_string(tracer));
            }
        }
    }
    auto scope = yama_ptrace_scope();
    if (scope) {
        st.ptrace_scope = *scope;
        st.ptrace_scope_restricted = *scope >= 1;
        if (st.ptrace_scope_restricted) {
            st.signals.push_back("yama.ptrace_scope=" + std::to_string(*scope));
        }
    }
#  ifdef PR_GET_DUMPABLE
    int dumpable = ::prctl(PR_GET_DUMPABLE, 0, 0, 0, 0);
    st.dumpable = dumpable != 0;
    if (!st.dumpable) st.signals.push_back("PR_GET_DUMPABLE=0");
#  endif
    // Avoid actually calling TRACEME in inspect (side effects). Use TracerPid only.
    st.self_ptrace_blocked = st.tracer_present;
    return st;
#else
    return Result<DebugStatus>(st, "inspect_self requires Linux");
#endif
}

} // namespace real::linux::anti_debug
