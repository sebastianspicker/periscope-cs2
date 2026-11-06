#pragma once

#include <string>
#include <vector>

namespace sim {

struct ForensicResult {
    bool prefetchCleaned{};
    bool recentItemsCleaned{};
    bool muiCacheCleaned{};
    bool userAssistCleaned{};
    std::vector<std::string> errors;
};

class ForensicEngine {
public:
    void initialize() noexcept;
    /// Records a deterministic simulated cleanup outcome. No host artifacts
    /// are enumerated or modified; prefetch is an explicit simulation input.
    ForensicResult execute_cleanup(bool include_prefetch = false) noexcept;
    void shutdown() noexcept;

    bool initialized() const noexcept { return m_initialized; }

    // Exposed for educational unit tests and simulation scenarios.
    bool clean_prefetch() noexcept;
    bool clean_recent_items() noexcept;
    bool clean_mui_cache() noexcept;
    bool clean_user_assist() noexcept;

private:
    bool m_initialized{};
};

} // namespace sim
