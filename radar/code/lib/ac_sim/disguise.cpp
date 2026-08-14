#include "ac_sim/disguise.hpp"

#include "ac_sim/xorshift.hpp"
#include "real/platform.hpp"

#include <cstdio>
#include <cstring>

#if LR_PLATFORM_WINDOWS
#  include "real/win/api_table.hpp"
#  include "real/win/windows_h.hpp"
#endif

namespace sim {

void DisguiseEngine::initialize() noexcept {
    initialize(DisguiseStyle::Afterburner, 0);
}

void DisguiseEngine::initialize(DisguiseStyle style, std::uint64_t seed) noexcept {
    m_style = style;
    if (seed == 0) {
        if (XorShiftPool::seeded()) {
            seed = XorShiftPool::instance(XorShiftInstance::Semantic).next();
        } else {
            seed = 0xD15A15Eu;
        }
    }
    m_seed = seed ? seed : 1;
    m_hasPrevTimes = false;
    m_initialized = true;
}

std::uint64_t DisguiseEngine::next_u64() noexcept {
    if (XorShiftPool::seeded()) {
        return XorShiftPool::instance(XorShiftInstance::Semantic).next();
    }
    m_seed = m_seed * 6364136223846793005ULL + 1ULL;
    return m_seed;
}

float DisguiseEngine::jitter_metric(float base, float variance) noexcept {
    const float u = static_cast<float>(next_u64() & 0x7FFFu) / 32768.0f;
    float result = base + (u - 0.5f) * 2.0f * variance;
    if (result < 0.f) result = 0.f;
    return result;
}

DisguiseMetrics DisguiseEngine::sample() noexcept {
    DisguiseMetrics metrics{};

#if LR_PLATFORM_WINDOWS
    auto& api = real::win::g_Api();
    if (api.GetSystemTimes) {
        FILETIME idle{}, kernel{}, user{};
        if (api.GetSystemTimes(&idle, &kernel, &user)) {
            metrics.idleTime =
                (static_cast<std::uint64_t>(idle.dwHighDateTime) << 32) |
                idle.dwLowDateTime;
            metrics.kernelTime =
                (static_cast<std::uint64_t>(kernel.dwHighDateTime) << 32) |
                kernel.dwLowDateTime;
            metrics.userTime =
                (static_cast<std::uint64_t>(user.dwHighDateTime) << 32) |
                user.dwLowDateTime;

            if (m_hasPrevTimes) {
                const double dIdle =
                    static_cast<double>(metrics.idleTime - m_prevIdle);
                const double dKernel =
                    static_cast<double>(metrics.kernelTime - m_prevKernel);
                const double dUser =
                    static_cast<double>(metrics.userTime - m_prevUser);
                const double total = dKernel + dUser;
                if (total > 1.0) {
                    metrics.cpuLoadPct =
                        static_cast<float>((1.0 - dIdle / total) * 100.0);
                    if (metrics.cpuLoadPct < 0.f) metrics.cpuLoadPct = 0.f;
                    if (metrics.cpuLoadPct > 100.f) metrics.cpuLoadPct = 100.f;
                }
            }
            m_prevIdle = metrics.idleTime;
            m_prevKernel = metrics.kernelTime;
            m_prevUser = metrics.userTime;
            m_hasPrevTimes = true;
        }
    }
    if (api.GlobalMemoryStatusEx) {
        MEMORYSTATUSEX ms{};
        ms.dwLength = sizeof(ms);
        if (api.GlobalMemoryStatusEx(&ms)) {
            metrics.totalPhysMB = ms.ullTotalPhys / (1024ull * 1024ull);
            metrics.availPhysMB = ms.ullAvailPhys / (1024ull * 1024ull);
        }
    }
#else
    // Synthetic lab metrics when Win32 APIs are unavailable.
    metrics.totalPhysMB = 16384;
    metrics.availPhysMB = 8192 + static_cast<std::uint64_t>(next_u64() % 2048);
    metrics.cpuLoadPct = jitter_metric(25.f, 10.f);
#endif

    // Smooth fake GPU metrics with small random walk.
    metrics.fakeGpuTemp = jitter_metric(72.f, 6.f);
    metrics.fakeGpuLoad = jitter_metric(42.f, 12.f);
    metrics.fakeGpuFan = jitter_metric(1400.f, 180.f);
    metrics.fakeFps = jitter_metric(144.f, 8.f);
    if (metrics.fakeFps < 30.f) metrics.fakeFps = 30.f;
    metrics.fakeFrameMs = 1000.f / metrics.fakeFps;

    metrics.osdText = render_osd_text(metrics);
    return metrics;
}

std::string DisguiseEngine::render_osd_text(const DisguiseMetrics& metrics) noexcept {
    char buf[320];
    switch (m_style) {
    case DisguiseStyle::Rtss:
        std::snprintf(buf, sizeof(buf),
                      "Framerate: %.0f FPS (%.2f ms)\n"
                      "GPU: %.0f C  %.0f%%  Fan %.0f RPM\n"
                      "RAM: %llu / %llu MB",
                      static_cast<double>(metrics.fakeFps),
                      static_cast<double>(metrics.fakeFrameMs),
                      static_cast<double>(metrics.fakeGpuTemp),
                      static_cast<double>(metrics.fakeGpuLoad),
                      static_cast<double>(metrics.fakeGpuFan),
                      static_cast<unsigned long long>(metrics.availPhysMB),
                      static_cast<unsigned long long>(metrics.totalPhysMB));
        break;
    case DisguiseStyle::GenericMonitor:
        std::snprintf(buf, sizeof(buf),
                      "CPU %.0f%% | GPU %.0fC %.0f%% | MEM %lluMB",
                      static_cast<double>(metrics.cpuLoadPct),
                      static_cast<double>(metrics.fakeGpuTemp),
                      static_cast<double>(metrics.fakeGpuLoad),
                      static_cast<unsigned long long>(metrics.availPhysMB));
        break;
    case DisguiseStyle::Afterburner:
    default:
        std::snprintf(buf, sizeof(buf),
                      "GPU %uC | Load %u%% | Fan %u RPM | "
                      "FPS %u | RAM %llu/%llu MB",
                      static_cast<unsigned>(metrics.fakeGpuTemp),
                      static_cast<unsigned>(metrics.fakeGpuLoad),
                      static_cast<unsigned>(metrics.fakeGpuFan),
                      static_cast<unsigned>(metrics.fakeFps),
                      static_cast<unsigned long long>(metrics.availPhysMB),
                      static_cast<unsigned long long>(metrics.totalPhysMB));
        break;
    }
    return std::string(buf);
}

void DisguiseEngine::shutdown() noexcept {
    m_initialized = false;
    m_hasPrevTimes = false;
}

} // namespace sim
