#include "ac_sim/dll_watch.hpp"

#include <algorithm>
#include <cctype>

namespace sim {

const char* kKnownAcPatterns[] = {
    "vapor", "vac", "steamservice", "punkbuster", "battleye", "beclient",
    "easyanticheat", "eac_", "easyac", "equ8", "faceit", "esportal",
    "medusa", "denuvo", "nprotect", "xigncode", "anticheat", "ac-",
    "crowdstrike", "csfalcon", "sentinel", "carbonblack", "vmware", "vbox",
    "wireshark", "cheatengine", "cheat eng", "tsearch", "reclass", "x64dbg",
    "x32dbg", "ollydbg", "ida64", "idaq", "ghidra", "processhacker",
    "procmon", "api monitor",
};
const std::size_t kKnownAcPatternsCount =
    sizeof(kKnownAcPatterns) / sizeof(kKnownAcPatterns[0]);

void DllWatch::initialize() noexcept {
    m_monitoring = false;
    m_cookie = nullptr;
    m_pending.clear();
    m_seen.clear();
}

void DllWatch::start_monitoring() noexcept { m_monitoring = true; }
void DllWatch::stop_monitoring() noexcept { m_monitoring = false; m_cookie = nullptr; }

void DllWatch::push_notification(const std::string& module_name) noexcept {
    DllNotification notification;
    notification.moduleName = module_name;
    notification.isLoad = true;
    if (const char* pattern = match_pattern(module_name.c_str())) {
        notification.isSuspicious = true;
        notification.matchedPattern = pattern;
    }
    push_notification(notification);
}

void DllWatch::push_notification(const DllNotification& notification) noexcept {
    std::lock_guard<std::mutex> lock(m_pendingMutex);
    m_pending.push_back(notification);
}

std::vector<DllNotification> DllWatch::pending_notifications() noexcept {
    std::lock_guard<std::mutex> lock(m_pendingMutex);
    auto result = std::move(m_pending);
    m_pending.clear();
    return result;
}

const char* DllWatch::match_pattern(const char* module_name) noexcept {
    if (!module_name) return nullptr;
    std::string name(module_name);
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    for (const char* pattern : kKnownAcPatterns) {
        if (name.find(pattern) != std::string::npos) return pattern;
    }
    return nullptr;
}

bool DllWatch::is_suspicious_module(const char* module_name) noexcept {
    return match_pattern(module_name) != nullptr;
}

int DllWatch::poll_modules() noexcept {
    // Host enumeration is an adapter concern. Tests inject replayable events.
    return 0;
}

void DllWatch::shutdown() noexcept {
    stop_monitoring();
    std::lock_guard<std::mutex> lock(m_pendingMutex);
    m_pending.clear();
    m_seen.clear();
}

} // namespace sim
