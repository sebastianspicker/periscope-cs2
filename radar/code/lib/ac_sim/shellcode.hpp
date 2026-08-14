#pragma once

// Position-independent x86-64 shellcode builder for PayloadComm-mediated
// NtReadVirtualMemory. Bytes are stored XOR-obfuscated; materialize()
// produces a runnable plaintext image only when the caller requests it.
//
// Size: 101 bytes (kShellcodeSize). Educational / lab use.
//
// CC-ledger CC.7

#include <cstddef>
#include <cstdint>

namespace sim {

static constexpr std::size_t kShellcodeSize = 101;
static constexpr std::uint32_t kPayloadMagic = 0x41434350u; // 'ACCP'

struct PayloadComm {
    std::uint32_t magic{kPayloadMagic};
    std::uint32_t command{0};
    std::uint64_t targetAddress{0};
    std::uint64_t readBuffer{0};
    std::uint32_t readSize{0};
    std::uint32_t status{0};
    std::uint64_t reserved[4]{};
};

struct ShellcodeResult {
    bool compiled{false};
    /// Always holds XOR-obfuscated bytes (never plaintext syscall image).
    std::uint8_t bytes[kShellcodeSize]{};
    std::size_t size{0};
    std::uint8_t xorKey{0};
    const char* error{nullptr};
};

class ShellcodeEngine {
public:
    /// Build the 101-byte PIC payload into an obfuscated buffer.
    /// xorKey: 0 → derive from build seed / RDTSC.
    ShellcodeResult build(std::uint8_t xorKey = 0) noexcept;

    /// Decode obfuscated blob into out[0..size). out must be >= kShellcodeSize.
    /// The materialized image is *unarmed* (NOP at syscall site).
    static bool Materialize(const ShellcodeResult& built, std::uint8_t* out,
                            std::size_t outCap) noexcept;

    /// Patch the syscall site in a materialized plaintext buffer to the real
    /// opcode pair. Split-written; lab-only — not called by default paths.
    static bool ArmSyscall(std::uint8_t* plain, std::size_t size) noexcept;

    /// Validate a *plaintext* (materialized) blob (armed or unarmed).
    static bool IsValidPlaintext(const void* code,
                                 std::size_t size = kShellcodeSize) noexcept;

    /// Validate an obfuscated result by materializing into a temp buffer.
    static bool IsValid(const ShellcodeResult& built) noexcept;
    static bool IsValid(const void* code, std::size_t size = kShellcodeSize) noexcept;
    static bool IsValid(void* code) noexcept {
        return IsValid(static_cast<const void*>(code));
    }

    static PayloadComm* SharedMemory() noexcept;

    static void Obfuscate(std::uint8_t* bytes, std::size_t size,
                          std::uint8_t key) noexcept;
    static void Deobfuscate(std::uint8_t* bytes, std::size_t size,
                            std::uint8_t key) noexcept;

    const ShellcodeResult& last() const noexcept { return m_last; }

    /// Structural invariants of a built blob (size, key, not all-zero).
    static bool StructuralOk(const ShellcodeResult& built) noexcept;

private:
    ShellcodeResult m_last{};

    static std::uint8_t derive_key() noexcept;
    static std::size_t emit_plaintext(std::uint8_t* out, std::size_t cap) noexcept;
};

} // namespace sim
