// aclab_client.cpp — CLI for the aclab kernel module (uses kmod_client library).
//
// BUILD: linked via CMake target aclab_client, or:
//   g++ -std=c++20 -I../../.. aclab_client.cpp kmod_client.cpp -o aclab_client
// RUN:   sudo ./aclab_client --scan

#include "real/linux/kmod_client.hpp"
#include "real/linux/aclab_ioctl.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>

#if defined(__linux__)
#  include <unistd.h>
#endif

using real::linux::kmod::Client;

static void cmd_info() {
    std::printf("\n=== aclab Kernel Module (ABI %u) ===\n", ACLAB_ABI_VERSION);
    std::printf("Device: %s\n", ACLAB_DEV_PATH);
    std::printf("IOCTL Interface:\n");
    std::printf("  PHYS_READ / PHYS_WRITE  physical memory via ioremap\n");
    std::printf("  GET_CR3                 process directory table base\n");
    std::printf("  VIRT_READ / VIRT_WRITE  process virtual memory\n");
    std::printf("  HIDE_PROC               DKOM task-list unlink\n");
    std::printf("  STEAL_CRED              credential copy (privesc lab)\n");
    std::printf("  MOD_INFO                ABI / build tag\n");
    std::printf("DETECT: lsmod | grep aclab; dmesg | grep aclab; lsof %s\n",
                ACLAB_DEV_PATH);
}

static void hex_dump(const uint8_t* data, size_t n, size_t max_show = 64) {
    size_t show = n < max_show ? n : max_show;
    for (size_t i = 0; i < show; ++i)
        std::printf("%02x ", data[i]);
    if (n > max_show) std::printf("...");
    std::printf("\n");
}

static int cmd_scan() {
    std::printf("\n=== aclab Module IOCTL Test Suite ===\n");
    Client c;
    auto o = c.open();
    if (!o) {
        std::printf("[!] %s\n", o.error_msg.c_str());
        return 1;
    }

    int fails = 0;

    {
        auto mi = c.info();
        if (mi) {
            std::printf("[Test 0] MOD_INFO: ABI=%u tag=%s\n",
                        mi->abi_version, mi->build_tag.c_str());
            if (!real::linux::kmod::abi_compatible(mi->abi_version) &&
                mi->abi_version != 0) {
                std::printf("  WARN: ABI mismatch (want %u)\n", ACLAB_ABI_VERSION);
            }
        } else {
            std::printf("[Test 0] MOD_INFO: soft-fail (%s)\n", mi.error_msg.c_str());
        }
    }

    {
        std::printf("[Test 1] PHYS_READ 0xE0000:\n");
        auto r = c.phys_read(0xE0000, 64);
        if (r) {
            std::printf("  PASS ");
            hex_dump(r->data(), r->size());
        } else {
            std::printf("  FAIL %s\n", r.error_msg.c_str());
            ++fails;
        }
    }

#if defined(__linux__)
    pid_t self = getpid();
#else
    int self = 0;
#endif
    {
        std::printf("[Test 2] GET_CR3 pid=%d:\n", (int)self);
        auto cr3 = c.get_cr3(static_cast<uint32_t>(self));
        if (cr3) {
            std::printf("  PASS CR3=0x%llx\n", (unsigned long long)*cr3);
        } else {
            std::printf("  FAIL %s\n", cr3.error_msg.c_str());
            ++fails;
        }
    }

    {
        uint64_t stack_var = 0x12345678ABCDEF00ULL;
        std::printf("[Test 3] VIRT_READ self stack:\n");
        auto r = c.virt_read(static_cast<uint32_t>(self),
                             reinterpret_cast<uint64_t>(&stack_var), 8);
        if (r && r->size() >= 8) {
            uint64_t val = 0;
            std::memcpy(&val, r->data(), 8);
            if (val == stack_var) {
                std::printf("  PASS value=0x%llx\n", (unsigned long long)val);
            } else {
                std::printf("  FAIL mismatch got=0x%llx\n",
                            (unsigned long long)val);
                ++fails;
            }
        } else {
            std::printf("  FAIL %s\n", r ? "short" : r.error_msg.c_str());
            ++fails;
        }
    }

    c.close();
    std::printf("\nScan complete: %d failures\n", fails);
    return fails ? 1 : 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("Usage:\n");
        std::printf("  %s --info\n", argv[0]);
        std::printf("  %s --scan\n", argv[0]);
        std::printf("  %s --phys 0xADDR [size]\n", argv[0]);
        std::printf("  %s --cr3 <pid>\n", argv[0]);
        std::printf("  %s --virt <pid> 0xADDR [size]\n", argv[0]);
        return 0;
    }

    if (std::strcmp(argv[1], "--info") == 0) {
        cmd_info();
        return 0;
    }
    if (std::strcmp(argv[1], "--scan") == 0) return cmd_scan();

    Client c;
    if (!c.open()) {
        std::printf("[!] Cannot open %s\n", ACLAB_DEV_PATH);
        return 1;
    }

    if (std::strcmp(argv[1], "--phys") == 0 && argc > 2) {
        uint64_t addr = std::strtoull(argv[2], nullptr, 0);
        size_t size = argc > 3 ? std::strtoul(argv[3], nullptr, 0) : 64;
        auto r = c.phys_read(addr, size);
        if (!r) {
            std::printf("[!] %s\n", r.error_msg.c_str());
            return 1;
        }
        hex_dump(r->data(), r->size());
        return 0;
    }
    if (std::strcmp(argv[1], "--cr3") == 0 && argc > 2) {
        auto cr3 = c.get_cr3(static_cast<uint32_t>(std::atoi(argv[2])));
        if (!cr3) {
            std::printf("[!] %s\n", cr3.error_msg.c_str());
            return 1;
        }
        std::printf("CR3=0x%llx\n", (unsigned long long)*cr3);
        return 0;
    }
    if (std::strcmp(argv[1], "--virt") == 0 && argc > 3) {
        uint32_t pid = static_cast<uint32_t>(std::atoi(argv[2]));
        uint64_t addr = std::strtoull(argv[3], nullptr, 0);
        size_t size = argc > 4 ? std::strtoul(argv[4], nullptr, 0) : 64;
        auto r = c.virt_read(pid, addr, size);
        if (!r) {
            std::printf("[!] %s\n", r.error_msg.c_str());
            return 1;
        }
        hex_dump(r->data(), r->size());
        return 0;
    }

    std::printf("Unknown command: %s\n", argv[1]);
    return 1;
}
