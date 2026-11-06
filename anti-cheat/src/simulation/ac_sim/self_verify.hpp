#pragma once

// Self-verify engine — CRC32 of image header / .text prologue and page
// permission scan (flags unexpected RWX regions).
//
// CC-ledger CC.15

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sim {

struct SelfVerifyResult {
    bool textCrcMatch{false};
    std::uint32_t computedCrc{0};
    std::uint32_t expectedCrc{0};
    bool pagePermissionsOk{false};
    std::vector<std::string> suspiciousPages;
    bool passed{false};
    bool baselineCaptured{false};
};

class SelfVerify {
public:
    /// Capture baseline CRC of the first 256 bytes of the image (or provided
    /// buffer). Must be called once before verify() for textCrcMatch to pass.
    void initialize() noexcept;

    /// Baseline from an explicit memory range (unit tests / custom regions).
    void initialize_from(const void* data, std::size_t size) noexcept;

    SelfVerifyResult verify() noexcept;

    void set_expected_crc(std::uint32_t crc) noexcept {
        m_expectedCrc = crc;
        m_hasBaseline = true;
    }

    std::uint32_t expected_crc() const noexcept { return m_expectedCrc; }
    bool has_baseline() const noexcept { return m_hasBaseline; }

    /// Public CRC helper — pure function used by initialize/verify and tests.
    static std::uint32_t crc32(const void* data, std::size_t size) noexcept;

private:
    std::uint32_t m_expectedCrc{0};
    bool m_hasBaseline{false};

    bool scan_page_permissions(std::vector<std::string>* outPages) noexcept;
    bool capture_image_baseline() noexcept;
};

} // namespace sim
