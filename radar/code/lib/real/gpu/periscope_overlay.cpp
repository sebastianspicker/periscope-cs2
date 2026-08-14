// Split from periscope_overlay.cpp — see MONOLITH_REFACTOR_LEDGER.
#include "real/gpu/periscope_overlay.hpp"
#include "real/win/peb_util.hpp"
#include "real/win/timing.hpp"
#include "real/win/xorstr.hpp"

#if LR_PLATFORM_WINDOWS
#include "real/win/windows_h.hpp"
#include "real/win/api_table.hpp"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_2.h>
#if LR_COMPILER_MSVC
#include <intrin.h>
#endif
#pragma comment(lib, "d3dcompiler.lib")
#endif

#include <cstring>
#include <cstdio>
#include <cstdint>
#include <random>
#include <vector>

namespace real::gpu::periscope {

// Generate random name from seed + prefix content.
// Hash the prefix bytes (not the pointer) so identical seed+prefix is
// deterministic across translation units and string pooling modes.
std::string generate_random_name(uint64_t seed, const char* prefix) noexcept {
    const char* p = prefix ? prefix : "MpsSvc";
    uint64_t h = seed ^ 0x9E3779B97F4A7C15ULL;
    for (const unsigned char* c = reinterpret_cast<const unsigned char*>(p); *c; ++c) {
        h ^= static_cast<uint64_t>(*c);
        h *= 0x100000001B3ULL;  // FNV-1a style mix
    }
    std::mt19937_64 rng(h);
    char buf[64]{};
    uint64_t rand_val = rng();
    std::snprintf(buf, sizeof(buf), "%s_%016llX",
                  p, static_cast<unsigned long long>(rand_val));
    return std::string(buf);
}

} // namespace real::gpu::periscope

