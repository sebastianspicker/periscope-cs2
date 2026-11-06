#pragma once

// HW-monitor disguise — generates deterministic synthetic CPU/RAM metrics and
// RTSS/Afterburner-style OSD text (GPU temp/load/fan + FPS).
//
// CC-ledger CC.8

#include <cstdint>
#include <string>

namespace sim {

struct DisguiseMetrics {
    std::uint64_t idleTime{0};
    std::uint64_t kernelTime{0};
    std::uint64_t userTime{0};
    std::uint64_t totalPhysMB{0};
    std::uint64_t availPhysMB{0};
    float cpuLoadPct{0.f};
    float fakeGpuTemp{75.0f};
    float fakeGpuLoad{45.0f};
    float fakeGpuFan{1200.0f};
    float fakeFps{144.0f};
    float fakeFrameMs{6.9f};
    std::string osdText;
};

enum class DisguiseStyle : std::uint8_t {
    Afterburner = 0,
    Rtss,
    GenericMonitor
};

class DisguiseEngine {
public:
    void initialize() noexcept;
    void initialize(DisguiseStyle style, std::uint64_t seed) noexcept;

    DisguiseMetrics sample() noexcept;
    std::string render_osd_text(const DisguiseMetrics& metrics) noexcept;

    void set_style(DisguiseStyle style) noexcept { m_style = style; }
    DisguiseStyle style() const noexcept { return m_style; }
    bool initialized() const noexcept { return m_initialized; }

    void shutdown() noexcept;

private:
    bool m_initialized{false};
    std::uint64_t m_seed{1};
    DisguiseStyle m_style{DisguiseStyle::Afterburner};
    std::uint64_t m_prevIdle{0};
    std::uint64_t m_prevKernel{0};
    std::uint64_t m_prevUser{0};
    bool m_hasPrevTimes{false};

    float jitter_metric(float base, float variance) noexcept;
    std::uint64_t next_u64() noexcept;
};

} // namespace sim
