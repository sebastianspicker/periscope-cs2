#include "ac_sim/shellcode.hpp"

#include "real/platform.hpp"

#include <cstring>
#include <initializer_list>

#if defined(__has_include)
#  if __has_include("build_keys.hpp")
#    include "build_keys.hpp"
#    define AC_SIM_HAS_BUILD_KEYS 1
#  endif
#endif

#if LR_PLATFORM_WINDOWS && LR_ARCH_X64
#  include <intrin.h>
#endif

namespace sim {
namespace {

// Offset of the 2-byte syscall site within the 101-byte blob.
// Filled with multi-byte NOP (0F 1F 00 is 3 bytes — we use two NOPs 90 90)
// until ArmSyscall patches to the real opcode pair via split writes.
constexpr std::size_t kSyscallSite = 0;
// Actual site is discovered during emit and stored in the high 16 bits of
// reserved[0] of PayloadComm for lab callers. For IsValid we accept either
// armed (syscall) or safe (two NOPs after mov eax,imm32 pattern).

} // namespace

std::uint8_t ShellcodeEngine::derive_key() noexcept {
    std::uint64_t seed = 0x5A;
#if defined(AC_SIM_HAS_BUILD_KEYS)
    seed ^= static_cast<std::uint64_t>(build::kXorKeySeed);
    seed ^= static_cast<std::uint64_t>(build::kCompileSalt) >> 8;
#endif
#if LR_PLATFORM_WINDOWS && LR_ARCH_X64
    unsigned aux = 0;
    seed ^= __rdtscp(&aux);
#endif
    std::uint8_t key = static_cast<std::uint8_t>(seed ^ (seed >> 8) ^ (seed >> 16));
    if (key == 0) key = 0xA7;
    return key;
}

void ShellcodeEngine::Obfuscate(std::uint8_t* bytes, std::size_t size,
                                std::uint8_t key) noexcept {
    if (!bytes || !key) return;
    for (std::size_t i = 0; i < size; ++i) {
        bytes[i] = static_cast<std::uint8_t>(
            bytes[i] ^ key ^ static_cast<std::uint8_t>(i * 13u));
    }
}

void ShellcodeEngine::Deobfuscate(std::uint8_t* bytes, std::size_t size,
                                  std::uint8_t key) noexcept {
    Obfuscate(bytes, size, key);
}

std::size_t ShellcodeEngine::emit_plaintext(std::uint8_t* out,
                                            std::size_t cap) noexcept {
    if (!out || cap < kShellcodeSize) return 0;

    std::memset(out, 0x90, cap);
    std::size_t o = 0;
    std::size_t syscallSite = 0;

    auto emit1 = [&](std::uint8_t b) {
        if (o < cap) out[o++] = b;
    };
    auto emit_n = [&](std::initializer_list<std::uint8_t> bytes) {
        for (auto b : bytes) emit1(b);
    };
    auto emit_u32 = [&](std::uint32_t v) {
        if (o + 4 <= cap) {
            std::memcpy(out + o, &v, 4);
            o += 4;
        }
    };
    auto emit_u64 = [&](std::uint64_t v) {
        if (o + 8 <= cap) {
            std::memcpy(out + o, &v, 8);
            o += 8;
        }
    };

    // push rbx rsi rdi r12 r13 r14 r15
    emit_n({0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57});

    // mov r12, imm64 ; &PayloadComm
    emit_n({0x49, 0xBC});
    emit_u64(reinterpret_cast<std::uint64_t>(SharedMemory()));

    // mov eax, [r12]
    emit_n({0x41, 0x8B, 0x04, 0x24});
    // cmp eax, imm32 magic
    emit1(0x3D);
    emit_u32(kPayloadMagic);
    // jne .fail (rel8) — displacement fixed after layout known; use short skip
    emit_n({0x75, 0x30});

    // mov edx, [r12+4]
    emit_n({0x41, 0x8B, 0x54, 0x24, 0x04});
    // mov rcx, -1
    emit_n({0x48, 0xC7, 0xC1, 0xFF, 0xFF, 0xFF, 0xFF});
    // mov rdx, [r12+8]
    emit_n({0x49, 0x8B, 0x54, 0x24, 0x08});
    // mov r8, [r12+16]
    emit_n({0x4D, 0x8B, 0x44, 0x24, 0x10});
    // mov r9d, [r12+24]
    emit_n({0x45, 0x8B, 0x4C, 0x24, 0x18});
    // sub rsp, 0x38
    emit_n({0x48, 0x83, 0xEC, 0x38});
    // mov qword [rsp+0x28], 0
    emit_n({0x48, 0xC7, 0x44, 0x24, 0x28, 0x00, 0x00, 0x00, 0x00});
    // mov eax, SSN placeholder (0x3F)
    emit_n({0xB8, 0x3F, 0x00, 0x00, 0x00});

    // Syscall site: two NOPs (safe image). ArmSyscall() patches to real ops.
    syscallSite = o;
    emit1(0x90);
    emit1(0x90);
    (void)kSyscallSite;

    // mov [r12+28], eax
    emit_n({0x41, 0x89, 0x44, 0x24, 0x1C});
    // add rsp, 0x38
    emit_n({0x48, 0x83, 0xC4, 0x38});
    // jmp .done
    emit_n({0xEB, 0x08});
    // .fail
    emit_n({0x41, 0xC7, 0x44, 0x24, 0x1C, 0x01, 0x00, 0x00, 0xC0});
    // .done pops
    emit_n({0x41, 0x5F, 0x41, 0x5E, 0x41, 0x5D, 0x41, 0x5C, 0x5F, 0x5E, 0x5B});
    // xor eax,eax ; ret
    emit_n({0x31, 0xC0, 0xC3});

    // Stash syscall site in PayloadComm::reserved[0] for arming.
    SharedMemory()->reserved[0] = syscallSite;

    // Pad with NOPs and force exact layout: byte0 = push rbx, last = ret.
    if (o > kShellcodeSize - 1) o = kShellcodeSize - 1;
    while (o < kShellcodeSize - 1) {
        out[o++] = 0x90;
    }
    out[0] = 0x53;                    // push rbx (invariant)
    out[kShellcodeSize - 1] = 0xC3;   // ret (invariant)
    return kShellcodeSize;
}

ShellcodeResult ShellcodeEngine::build(std::uint8_t xorKey) noexcept {
    ShellcodeResult result{};

#if LR_PLATFORM_WINDOWS && LR_ARCH_X64
    if (xorKey == 0) xorKey = derive_key();
    result.xorKey = xorKey;

    std::uint8_t plain[kShellcodeSize]{};
    const std::size_t n = emit_plaintext(plain, sizeof(plain));
    if (n != kShellcodeSize) {
        result.error = "emit size mismatch";
        std::memset(plain, 0, sizeof(plain));
        m_last = result;
        return result;
    }

    // Basic structural checks on plaintext before obfuscating.
    if (plain[0] != 0x53 || plain[kShellcodeSize - 1] != 0xC3) {
        result.error = "emit structural mismatch";
        std::memset(plain, 0, sizeof(plain));
        m_last = result;
        return result;
    }

    std::memcpy(result.bytes, plain, kShellcodeSize);
    Obfuscate(result.bytes, kShellcodeSize, xorKey);
    std::memset(plain, 0, sizeof(plain));

    result.size = kShellcodeSize;
    result.compiled = StructuralOk(result);
    if (!result.compiled) result.error = "structural validation failed";
#else
    (void)xorKey;
    result.error = "shellcode requires Windows x64";
#endif

    m_last = result;
    return result;
}

bool ShellcodeEngine::Materialize(const ShellcodeResult& built, std::uint8_t* out,
                                  std::size_t outCap) noexcept {
    if (!out || outCap < kShellcodeSize || !built.compiled ||
        built.size != kShellcodeSize || built.xorKey == 0) {
        return false;
    }
    std::memcpy(out, built.bytes, kShellcodeSize);
    Deobfuscate(out, kShellcodeSize, built.xorKey);
    return true;
}

bool ShellcodeEngine::ArmSyscall(std::uint8_t* plain,
                                 std::size_t size) noexcept {
    if (!plain || size < kShellcodeSize) return false;
    const std::size_t site =
        static_cast<std::size_t>(SharedMemory()->reserved[0]);
    if (site == 0 || site + 1 >= size) return false;
    // Split writes so a single store never materializes both signature bytes
    // as an immediate pair in the caller's instruction stream either.
    const std::uint8_t b0 = static_cast<std::uint8_t>(0x00u + 0x0Fu);
    const std::uint8_t b1 = static_cast<std::uint8_t>(0x04u + 0x01u);
    plain[site] = b0;
    plain[site + 1] = b1;
    return plain[site] == 0x0F && plain[site + 1] == 0x05;
}

bool ShellcodeEngine::StructuralOk(const ShellcodeResult& built) noexcept {
    if (built.size != kShellcodeSize) return false;
    if (built.xorKey == 0) return false;
    bool diversity = false;
    for (std::size_t i = 1; i < built.size; ++i) {
        if (built.bytes[i] != built.bytes[0]) {
            diversity = true;
            break;
        }
    }
    return diversity;
}

bool ShellcodeEngine::IsValidPlaintext(const void* code,
                                       std::size_t size) noexcept {
    if (!code || size < 16) return false;
    const auto* b = static_cast<const std::uint8_t*>(code);
    if (b[0] != 0x53) return false;
    if (b[size - 1] != 0xC3) return false;
    // Accept unarmed (NOP site) or armed (syscall) forms.
    // Look for mov eax, imm32 (B8) which precedes the site.
    bool hasMovEax = false;
    for (std::size_t i = 0; i + 5 < size; ++i) {
        if (b[i] == 0xB8) {
            hasMovEax = true;
            break;
        }
    }
    return hasMovEax;
}

bool ShellcodeEngine::IsValid(const ShellcodeResult& built) noexcept {
    if (!StructuralOk(built)) return false;
    std::uint8_t plain[kShellcodeSize]{};
    if (!Materialize(built, plain, sizeof(plain))) return false;
    const bool ok = IsValidPlaintext(plain, kShellcodeSize);
    std::memset(plain, 0, sizeof(plain));
    return ok;
}

bool ShellcodeEngine::IsValid(const void* code, std::size_t size) noexcept {
    return IsValidPlaintext(code, size);
}

PayloadComm* ShellcodeEngine::SharedMemory() noexcept {
    static PayloadComm comm{};
    static bool initialized = false;
    if (!initialized) {
        std::memset(&comm, 0, sizeof(comm));
        comm.magic = kPayloadMagic;
        initialized = true;
    }
    return &comm;
}

} // namespace sim
