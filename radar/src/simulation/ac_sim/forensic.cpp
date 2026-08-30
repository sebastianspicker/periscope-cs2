#include "ac_sim/forensic.hpp"

namespace sim {

void ForensicEngine::initialize() noexcept { m_initialized = true; }

ForensicResult ForensicEngine::execute_cleanup(bool include_prefetch) noexcept {
    ForensicResult result{};
    if (!m_initialized) {
        result.errors.emplace_back("not initialized");
        return result;
    }
    // Simulation records this outcome only. It never inspects or mutates host state.
    result.prefetchCleaned = include_prefetch;
    result.recentItemsCleaned = true;
    result.muiCacheCleaned = true;
    result.userAssistCleaned = true;
    return result;
}

void ForensicEngine::shutdown() noexcept { m_initialized = false; }
bool ForensicEngine::clean_prefetch() noexcept { return m_initialized; }
bool ForensicEngine::clean_recent_items() noexcept { return m_initialized; }
bool ForensicEngine::clean_mui_cache() noexcept { return m_initialized; }
bool ForensicEngine::clean_user_assist() noexcept { return m_initialized; }

} // namespace sim
