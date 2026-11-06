#pragma once

// System normalization — deterministic WER, handle-count, and power-request
// simulation signals. It never changes host policy or process state.
//
// CC-ledger CC.12

#include <cstdint>

namespace sim {

struct NormalizeResult {
    bool werSuppressed{false};
    bool handleCountSampled{false};
    bool handleCountNormalized{false}; // true when sampling succeeded (compat)
    bool powerRequestAcquired{false};
    int handleCount{0};
    const char* error{nullptr};
};

struct NormalizeConfig {
    bool suppressGpfBox{true};
    bool sampleHandleCount{true};
    bool acquirePowerRequest{false}; // off by default — detection surface
    int minReportedHandles{0};       // informational only
};

class SystemNormalizer {
public:
    void initialize() noexcept;
    void initialize(const NormalizeConfig& cfg) noexcept;

    NormalizeResult normalize() noexcept;
    void shutdown() noexcept;

    int handle_count() const noexcept { return m_handleCount; }
    bool initialized() const noexcept { return m_initialized; }

private:
    bool m_initialized{false};
    int m_handleCount{0};
    NormalizeConfig m_config{};
    bool suppress_windows_error_reporting() noexcept;
    bool sample_handle_count() noexcept;
    bool acquire_foreground_power_request() noexcept;
    void release_power_request() noexcept;
};

} // namespace sim
