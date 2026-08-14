#include "ac_sim/system_normalize.hpp"

#include "real/platform.hpp"

#include <cstring>

#if LR_PLATFORM_WINDOWS
#  include "real/win/api_table.hpp"
#  include "real/win/windows_h.hpp"
#endif

namespace sim {

void SystemNormalizer::initialize() noexcept {
    initialize(NormalizeConfig{});
}

void SystemNormalizer::initialize(const NormalizeConfig& cfg) noexcept {
    m_config = cfg;
    m_handleCount = 0;
    m_initialized = true;
}

NormalizeResult SystemNormalizer::normalize() noexcept {
    NormalizeResult result{};
    if (!m_initialized) {
        result.error = "not initialized";
        return result;
    }

    if (m_config.suppressGpfBox) {
        result.werSuppressed = suppress_windows_error_reporting();
    }
    if (m_config.sampleHandleCount) {
        result.handleCountSampled = sample_handle_count();
        result.handleCountNormalized = result.handleCountSampled;
    }
    if (m_config.acquirePowerRequest) {
        result.powerRequestAcquired = acquire_foreground_power_request();
    } else {
        // Report success for the no-op path so callers treating this as
        // "policy applied" are not falsely failed.
        result.powerRequestAcquired = true;
    }
    result.handleCount = m_handleCount;
    return result;
}

void SystemNormalizer::shutdown() noexcept {
    release_power_request();
    m_initialized = false;
}

bool SystemNormalizer::suppress_windows_error_reporting() noexcept {
#if LR_PLATFORM_WINDOWS
    // Only SEM_NOGPFAULTERRORBOX — other SEM_* flags are cheat signatures.
    const UINT prev = ::SetErrorMode(0);
    ::SetErrorMode(prev | SEM_NOGPFAULTERRORBOX);
    return true;
#else
    return true; // no-op success on non-Windows lab hosts
#endif
}

bool SystemNormalizer::sample_handle_count() noexcept {
#if LR_PLATFORM_WINDOWS
    // Read-only sampling via GetProcessHandleCount — does NOT open self-dup
    // handles (that anti-forensics trick is now a detection signal).
    DWORD count = 0;
    if (::GetProcessHandleCount(::GetCurrentProcess(), &count)) {
        m_handleCount = static_cast<int>(count);
        return true;
    }
    // Fallback: NtQueryInformationProcess HandleCount (class 20) if available.
    auto& api = real::win::g_Api();
    if (api.NtQueryInformationProcess) {
        ULONG handleCount = 0;
        ULONG retLen = 0;
        // ProcessHandleCount = 20
        const LONG st = api.NtQueryInformationProcess(
            ::GetCurrentProcess(), 20, &handleCount, sizeof(handleCount),
            &retLen);
        if (st >= 0) {
            m_handleCount = static_cast<int>(handleCount);
            return true;
        }
    }
    m_handleCount = 0;
    return false;
#else
    m_handleCount = 32; // synthetic
    return true;
#endif
}

bool SystemNormalizer::acquire_foreground_power_request() noexcept {
#if LR_PLATFORM_WINDOWS
    // Optional: PowerCreateRequest + PowerSetRequest(DisplayRequired).
    // Off by default in NormalizeConfig because elevated power state is
    // itself a behavioral tell. Implemented fully for research when enabled.
    if (m_powerRequest) return true;

    // POWER_REQUEST_CONTEXT
    REASON_CONTEXT ctx{};
    ctx.Version = POWER_REQUEST_CONTEXT_VERSION;
    ctx.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
    ctx.Reason.SimpleReasonString = const_cast<wchar_t*>(L"Display");

    HANDLE req = ::PowerCreateRequest(&ctx);
    if (req == INVALID_HANDLE_VALUE || !req) {
        return false;
    }
    if (!::PowerSetRequest(req, PowerRequestDisplayRequired)) {
        ::CloseHandle(req);
        return false;
    }
    m_powerRequest = req;
    return true;
#else
    return true;
#endif
}

void SystemNormalizer::release_power_request() noexcept {
#if LR_PLATFORM_WINDOWS
    if (m_powerRequest) {
        ::PowerClearRequest(static_cast<HANDLE>(m_powerRequest),
                            PowerRequestDisplayRequired);
        ::CloseHandle(static_cast<HANDLE>(m_powerRequest));
        m_powerRequest = nullptr;
    }
#endif
}

} // namespace sim
