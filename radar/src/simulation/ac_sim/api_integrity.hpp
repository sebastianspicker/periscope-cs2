#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace sim {

enum class HookType {
    None,
    IatHook,
    EatHook,
    InlineHook,
    Unknown
};

struct ApiIntegrityResult {
    std::string moduleName;
    std::string functionName;
    uint64_t resolvedAddress{};
    uint64_t expectedAddress{};
    HookType hookType{HookType::None};
    bool passed{true};
    std::string detail;
};

class ApiIntegrityChecker {
public:
    ApiIntegrityResult verify(const char* moduleName, const char* funcName,
                               uint64_t resolvedAddr) noexcept;

    std::vector<ApiIntegrityResult> verify_all() noexcept;

    void set_known_good(const char* moduleName, uint64_t baseAddr,
                         uint64_t size) noexcept;

private:
    struct KnownModule {
        std::string name;
        uint64_t base{};
        uint64_t size{};
    };
    std::vector<KnownModule> m_knownModules;

    bool is_known_module(uint64_t addr, uint64_t& baseOut) noexcept;
    bool detect_inline_hook(uint64_t funcAddr) noexcept;
};

} // namespace sim
