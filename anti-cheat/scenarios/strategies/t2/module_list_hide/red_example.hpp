#pragma once
#include "ac/types.hpp"
#include "sim/world.hpp"
#include <string>

namespace strategy::t2_module_list_hide {

struct RedResult {
    bool module_hidden{false};
    bool pe_header_still_visible{false};
    int modules_before{};
    int modules_after{};
    std::string detail;
};

class Red {
public:
    void apply(sim::World& w) noexcept;
    static constexpr const char* kDescription =
        "Hide loaded module from CModuleListSnapshot enumeration via "
        "DKOM-style PEB Ldr unlink. Uses kernel driver to remove the "
        "LDR_DATA_TABLE_ENTRY from the InLoadOrderModuleList, making "
        "EnumProcessModulesEx miss it.";
};

} // namespace
