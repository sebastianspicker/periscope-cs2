#pragma once

// DLL load monitor (blue-team surface) — replay-injected notifications are
// matched against known module-name patterns. Host enumeration belongs to a
// real adapter.
//
// CC-ledger CC.14

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace sim {

struct DllNotification {
    std::uint64_t baseAddress{0};
    std::string moduleName;
    std::string fullPath;
    bool isLoad{true};
    bool isSuspicious{false};
    std::string matchedPattern;
};

extern const char* kKnownAcPatterns[];
extern const std::size_t kKnownAcPatternsCount;

class DllWatch {
public:
    void initialize() noexcept;
    void start_monitoring() noexcept;
    void stop_monitoring() noexcept;

    /// Drain pending load notifications (from callback or inject).
    std::vector<DllNotification> pending_notifications() noexcept;

    bool is_suspicious_module(const char* moduleName) noexcept;
    /// Returns matched pattern or empty string.
    const char* match_pattern(const char* moduleName) noexcept;

    void push_notification(const std::string& modName) noexcept;
    void push_notification(const DllNotification& n) noexcept;

    /// Simulation has no host module list; injected events are replayable.
    int poll_modules() noexcept;

    bool monitoring() const noexcept { return m_monitoring; }
    std::size_t known_pattern_count() const noexcept {
        return kKnownAcPatternsCount;
    }

    void shutdown() noexcept;

private:
    bool m_monitoring{false};
    void* m_cookie{nullptr};
    std::vector<DllNotification> m_pending;
    std::mutex m_pendingMutex;
    std::vector<std::string> m_seen; // de-dup poll results
};

} // namespace sim
