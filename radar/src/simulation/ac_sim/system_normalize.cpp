#include "ac_sim/system_normalize.hpp"

namespace sim {

void SystemNormalizer::initialize() noexcept { initialize(NormalizeConfig{}); }

void SystemNormalizer::initialize(const NormalizeConfig& config) noexcept {
    m_config = config;
    m_handleCount = 0;
    m_initialized = true;
}

NormalizeResult SystemNormalizer::normalize() noexcept {
    NormalizeResult result{};
    if (!m_initialized) {
        result.error = "not initialized";
        return result;
    }
    result.werSuppressed = m_config.suppressGpfBox;
    result.handleCountSampled = m_config.sampleHandleCount;
    result.handleCountNormalized = result.handleCountSampled;
    if (result.handleCountSampled) {
        m_handleCount = m_config.minReportedHandles > 0 ? m_config.minReportedHandles : 32;
    }
    result.handleCount = m_handleCount;
    result.powerRequestAcquired = !m_config.acquirePowerRequest;
    return result;
}

void SystemNormalizer::shutdown() noexcept { m_initialized = false; }
bool SystemNormalizer::suppress_windows_error_reporting() noexcept { return true; }
bool SystemNormalizer::sample_handle_count() noexcept { m_handleCount = 32; return true; }
bool SystemNormalizer::acquire_foreground_power_request() noexcept { return false; }
void SystemNormalizer::release_power_request() noexcept {}

} // namespace sim
