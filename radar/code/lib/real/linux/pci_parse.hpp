// pci_parse.hpp — Pure PCI sysfs resource / BDF parsers.
// Portable: no OS syscalls. Unit-tested on every host.

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>

namespace real::linux::pci {

// resource flags (linux/ioport.h / pci_resource flags subset)
constexpr uint64_t kIoResourceIo      = 0x00000100ULL; // IORESOURCE_IO
constexpr uint64_t kIoResourceMem     = 0x00000200ULL; // IORESOURCE_MEM
constexpr uint64_t kIoResourcePrefetch= 0x00002000ULL; // IORESOURCE_PREFETCH
constexpr uint64_t kIoResourceMem64   = 0x00100000ULL; // IORESOURCE_MEM_64
constexpr uint64_t kIoResourceReadonly= 0x00004000ULL;

struct Bdf {
    uint16_t domain = 0;
    uint8_t  bus = 0;
    uint8_t  device = 0;
    uint8_t  function = 0;
};

struct BarResource {
    uint64_t start = 0;
    uint64_t end = 0;
    uint64_t flags = 0;
    uint64_t size = 0; // end - start + 1 if valid
    bool valid = false;
    bool is_mem = false;
    bool is_io = false;
    bool is_64 = false;
    bool prefetchable = false;
};

/// Parse "DDDD:BB:DD.F" sysfs directory name.
inline bool parse_bdf(const char* name, Bdf& out) noexcept {
    if (!name) return false;
    unsigned domain = 0, bus = 0, device = 0, function = 0;
    if (std::sscanf(name, "%x:%x:%x.%x", &domain, &bus, &device, &function) != 4)
        return false;
    if (bus > 0xFF || device > 0x1F || function > 0x7) return false;
    out.domain = static_cast<uint16_t>(domain);
    out.bus = static_cast<uint8_t>(bus);
    out.device = static_cast<uint8_t>(device);
    out.function = static_cast<uint8_t>(function);
    return true;
}

inline bool parse_bdf(const std::string& name, Bdf& out) noexcept {
    return parse_bdf(name.c_str(), out);
}

/// Format BDF as sysfs-style "0000:00:1f.0".
inline std::string format_bdf(const Bdf& b) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04x:%02x:%02x.%x",
                  b.domain, b.bus, b.device, b.function);
    return buf;
}

/// Parse one line from /sys/bus/pci/devices/.../resource
/// Format: "0xSTART 0xEND 0xFLAGS"
inline bool parse_resource_line(const char* line, BarResource& out) noexcept {
    if (!line) return false;
    unsigned long long start = 0, end = 0, flags = 0;
    if (std::sscanf(line, "0x%llx 0x%llx 0x%llx", &start, &end, &flags) < 2 &&
        std::sscanf(line, "%llx %llx %llx", &start, &end, &flags) < 2) {
        return false;
    }
    out = BarResource{};
    out.start = static_cast<uint64_t>(start);
    out.end = static_cast<uint64_t>(end);
    out.flags = static_cast<uint64_t>(flags);
    if (end >= start && (start != 0 || end != 0)) {
        out.size = static_cast<uint64_t>(end - start + 1);
        out.valid = out.size > 0 && start != 0;
    }
    out.is_io = (out.flags & kIoResourceIo) != 0;
    out.is_mem = (out.flags & kIoResourceMem) != 0 ||
                 (!out.is_io && out.valid);
    out.is_64 = (out.flags & kIoResourceMem64) != 0;
    out.prefetchable = (out.flags & kIoResourcePrefetch) != 0;
    return true;
}

inline bool parse_resource_line(const std::string& line, BarResource& out) noexcept {
    return parse_resource_line(line.c_str(), out);
}

/// Parse hex sysfs attribute value ("0x8086" or "8086").
inline bool parse_hex_u64(const char* text, uint64_t& out) noexcept {
    if (!text || !*text) return false;
    unsigned long long v = 0;
    if (std::sscanf(text, "%llx", &v) != 1) return false;
    out = static_cast<uint64_t>(v);
    return true;
}

/// DMA-capable heuristic: memory BAR present (T4 peer-to-peer candidates).
inline bool is_dma_capable(const BarResource* bars, size_t count) noexcept {
    if (!bars) return false;
    for (size_t i = 0; i < count; ++i) {
        if (bars[i].valid && bars[i].is_mem && bars[i].size > 0) return true;
    }
    return false;
}

} // namespace real::linux::pci
