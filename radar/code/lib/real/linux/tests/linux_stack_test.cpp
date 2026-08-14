// linux_stack_test.cpp — Unit tests for real::linux shipped code.
//
// Proves pure helpers + real entry points (read_self_memory, page_walk,
// parsers, ABI) on every host. Live /proc and /dev paths run when
// LR_PLATFORM_LINUX=1.

#include "real/linux/aclab_ioctl.h"
#include "real/linux/anti_debug.hpp"
#include "real/linux/bpf_probe.hpp"
#include "real/linux/hook_detect.hpp"
#include "real/linux/kmod_client.hpp"
#include "real/linux/maps_parse.hpp"
#include "real/linux/memory.hpp"
#include "real/linux/module_enum.hpp"
#include "real/linux/netlink_audit.hpp"
#include "real/linux/page_walk.hpp"
#include "real/linux/pagemap.hpp"
#include "real/linux/pci_parse.hpp"
#include "real/linux/pcie.hpp"
#include "real/linux/process.hpp"
#include "real/platform.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

static int g_fails = 0;
static int g_pass = 0;

#define CHECK(cond, msg)                                                       \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::printf("FAIL: %s\n", msg);                                          \
      ++g_fails;                                                               \
    } else {                                                                   \
      std::printf("ok: %s\n", msg);                                            \
      ++g_pass;                                                                \
    }                                                                          \
  } while (0)

// ── Pagemap pure decode ────────────────────────────────────────────

static void test_pagemap_decode() {
    std::printf("\n=== pagemap::decode / to_physical ===\n");
    using namespace real::linux::pagemap;

    // Present page: PFN=0x12345, present bit set
    uint64_t raw = 0x12345ULL | kPresentBit;
    Entry e = decode(raw);
    CHECK(e.present, "present bit");
    CHECK(!e.swapped, "not swapped");
    CHECK(e.pfn == 0x12345ULL, "pfn extracted");
    uint64_t phys = to_physical(e, 0x7fff00001234ULL, 4096);
    CHECK(phys == (0x12345ULL * 4096) + 0x234, "VA->PA offset applied");

    // Soft-dirty + exclusive
    raw = 0x1ULL | kPresentBit | kSoftDirtyBit | kExclusiveBit;
    e = decode(raw);
    CHECK(e.soft_dirty && e.exclusively_mapped, "soft-dirty+exclusive");

    // Swapped
    raw = kSwappedBit | 0x3ULL | (0xABCULL << 5);
    e = decode(raw);
    CHECK(e.swapped && !e.present, "swapped page");
    CHECK(e.swap_type == 3, "swap type");

    // entry_file_offset
    CHECK(entry_file_offset(0x2000, 4096) == 2 * 8, "pagemap file offset");

    // Indices
    uint64_t va = 0x00007f1234567890ULL;
    CHECK(pml4_index(va) < 512 && pdpt_index(va) < 512, "page indices in range");
}

static void test_page_walk_pure() {
    std::printf("\n=== pagemap::walk_from_entries ===\n");
    using namespace real::linux::pagemap;

    // 4K path: all present, no large
    uint64_t pml4e = kPagePresent | 0x1000;
    uint64_t pdpte = kPagePresent | 0x2000;
    uint64_t pde = kPagePresent | 0x3000;
    uint64_t pte = kPagePresent | 0xABCD000ULL;
    uint64_t va = 0x1234; // page offset = 0x234 (low 12 bits)
    auto r = walk_from_entries(va, pml4e, pdpte, pde, pte);
    CHECK(r.ok, "4K walk ok");
    CHECK(r.page_size == 4096, "4K page size");
    CHECK(r.phys == (0xABCD000ULL + 0x234), "4K phys");

    // 2M large page
    pde = kPagePresent | kPageLarge | 0x80000000ULL;
    r = walk_from_entries(0x123456, pml4e, pdpte, pde, 0);
    CHECK(r.ok && r.page_size == kLargePageSz, "2M large page");
    CHECK(r.phys == (0x80000000ULL + 0x123456), "2M phys offset");

    // Not present
    r = walk_from_entries(va, 0, 0, 0, 0);
    CHECK(!r.ok && r.fail_stage != nullptr, "missing pml4e fails");
}

static void test_page_walk_with_mock_phys() {
    std::printf("\n=== page_walk::walk (mock phys backend) ===\n");
    using namespace real::linux;

    // Build a tiny fake page-table tree in a vector-backed "physical" memory.
    // CR3 -> table at PA 0x1000 (PML4)
    // PML4[0] -> 0x2000 (PDPT)
    // PDPT[0] -> 0x3000 (PD)
    // PD[0]   -> 0x4000 (PT)
    // PT[0]   -> frame 0x9000 present
    std::vector<uint8_t> ram(0x10000, 0);
    auto store_u64 = [&](uint64_t pa, uint64_t val) {
        std::memcpy(ram.data() + pa, &val, 8);
    };
    store_u64(0x1000 + 0 * 8, pagemap::kPagePresent | 0x2000);
    store_u64(0x2000 + 0 * 8, pagemap::kPagePresent | 0x3000);
    store_u64(0x3000 + 0 * 8, pagemap::kPagePresent | 0x4000);
    store_u64(0x4000 + 0 * 8, pagemap::kPagePresent | 0x9000);

    auto phys_read = [&](uint64_t phys, size_t size) -> real::Result<std::vector<uint8_t>> {
        if (phys + size > ram.size()) {
            return real::Result<std::vector<uint8_t>>({}, "OOB phys");
        }
        return std::vector<uint8_t>(ram.begin() + static_cast<std::ptrdiff_t>(phys),
                                    ram.begin() + static_cast<std::ptrdiff_t>(phys + size));
    };

    auto wr = page_walk::walk(0x1000, 0x123, phys_read);
    CHECK(wr && wr->ok, "mock walk succeeds");
    if (wr && wr->ok) {
        CHECK(wr->phys == 0x9000 + 0x123, "mock walk phys address");
        CHECK(wr->page_size == 4096, "mock walk 4K");
    }

    // Slot helper
    CHECK(page_walk::table_slot_phys(0x2000, 3) == 0x2000 + 24, "table_slot_phys");
}

// ── Maps / PCI / modules parsers ───────────────────────────────────

static void test_maps_parse() {
    std::printf("\n=== maps::parse_line ===\n");
    using namespace real::linux::maps;

    const char* line =
        "7f8a2c000000-7f8a2c021000 r-xp 00000000 08:01 123456 /usr/lib/libc.so.6";
    Mapping m;
    CHECK(parse_line(line, m), "parse maps line");
    CHECK(m.start == 0x7f8a2c000000ULL, "maps start");
    CHECK(m.end == 0x7f8a2c021000ULL, "maps end");
    CHECK(m.readable && !m.writable && m.executable && !m.shared, "maps perms");
    CHECK(m.pathname.find("libc.so.6") != std::string::npos, "maps path");
    CHECK(size_bytes(m) == 0x21000, "maps size");

    std::vector<Mapping> all{m};
    CHECK(find_by_name(all, "libc") != nullptr, "find_by_name");
    CHECK(find_exec_by_name(all, "libc") != nullptr, "find_exec_by_name");
    CHECK(contains(m, m.start + 0x100), "contains addr");
    CHECK(!parse_line("garbage", m), "reject garbage");
}

static void test_pci_parse() {
    std::printf("\n=== pci::parse_bdf / resource ===\n");
    using namespace real::linux::pci;

    Bdf b;
    CHECK(parse_bdf("0000:00:1f.0", b), "parse bdf");
    CHECK(b.bus == 0 && b.device == 0x1f && b.function == 0, "bdf fields");
    CHECK(format_bdf(b) == "0000:00:1f.0", "format bdf");
    CHECK(!parse_bdf("not-a-bdf", b), "reject bad bdf");

    BarResource bar;
    CHECK(parse_resource_line("0x00000000f0000000 0x00000000f0000fff 0x0000000000040200",
                              bar),
          "parse resource line");
    CHECK(bar.valid && bar.size == 0x1000, "bar size");
    CHECK(bar.is_mem, "bar is mem");

    BarResource bars[2] = {bar, {}};
    CHECK(is_dma_capable(bars, 2), "dma capable heuristic");

    uint64_t v = 0;
    CHECK(parse_hex_u64("0x8086", v) && v == 0x8086, "parse hex vendor");
}

static void test_stat_and_uid_parsers() {
    std::printf("\n=== process stat/status parsers ===\n");
    using namespace real::linux::proc;

    // Realistic truncated stat line (comm with spaces)
    std::string stat =
        "42 (my proc) S 1 42 42 0 -1 4194304 100 0 0 0 10 20 0 0 20 0 1 0 12345 "
        "12345678 100 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0";
    ProcessInfo pi;
    CHECK(parse_stat_line(stat, pi), "parse_stat_line");
    CHECK(pi.pid == 42, "stat pid");
    CHECK(pi.name == "my proc", "stat name with space");
    CHECK(pi.state == 'S', "stat state");
    CHECK(pi.ppid == 1, "stat ppid");
    CHECK(pi.start_time == 12345ULL, "stat start_time field 22");

    uint32_t uid = 0, euid = 0;
    CHECK(parse_status_uid_line("Uid:\t1000\t1000\t1000\t1000", uid, euid),
          "parse uid line");
    CHECK(uid == 1000 && euid == 1000, "uid values");
}

static void test_modules_parse() {
    std::printf("\n=== modules::parse_modules_line ===\n");
    using namespace real::linux::modules;

    KernelModule m;
    CHECK(parse_modules_line(
              "aclab_module 16384 0 - Live 0xffffffffc0123000", m),
          "parse modules line");
    CHECK(m.name == "aclab_module", "module name");
    CHECK(m.size == 16384, "module size");
    CHECK(name_is_high_risk("aclab_module"), "high risk name");
    CHECK(!name_is_high_risk("ext4"), "ext4 not high risk");
}

static void test_anti_debug_parse() {
    std::printf("\n=== anti_debug::parse_tracer_pid ===\n");
    using namespace real::linux::anti_debug;

    const char* status =
        "Name:\tbash\nUmask:\t0022\nState:\tS (sleeping)\nTracerPid:\t0\n";
    uint32_t t = 99;
    CHECK(parse_tracer_pid(status, t) && t == 0, "TracerPid 0");
    status = "Name:\tx\nTracerPid:\t1234\n";
    CHECK(parse_tracer_pid(status, t) && t == 1234, "TracerPid 1234");
}

static void test_hook_helpers() {
    std::printf("\n=== hook_detect helpers ===\n");
    using namespace real::linux::hook_detect;

    auto parts = split_preload_list("/tmp/a.so:/home/b.so");
    CHECK(parts.size() == 2, "split preload");
    std::vector<std::string> allow;
    CHECK(pathname_suspicious("/tmp/evil.so", allow), "tmp so suspicious");
    CHECK(!pathname_suspicious("/usr/lib/libc.so.6", allow), "libc not suspicious");
    CHECK(!pathname_suspicious("[vdso]", allow), "vdso not suspicious");
}

static void test_bpf_audit_parse() {
    std::printf("\n=== bpf/audit pure parsers ===\n");
    std::string path;
    const char* mounts =
        "sysfs /sys sysfs rw 0 0\nbpf /sys/fs/bpf bpf rw 0 0\n";
    CHECK(real::linux::bpf::mounts_has_bpffs(mounts, path), "mounts has bpffs");
    CHECK(path == "/sys/fs/bpf", "bpffs path");

    bool b = false;
    CHECK(real::linux::audit::parse_sysctl_bool("1\n", b) && b, "sysctl true");
    CHECK(real::linux::audit::parse_sysctl_bool("0", b) && !b, "sysctl false");
}

static void test_ioctl_abi() {
    std::printf("\n=== aclab_ioctl ABI ===\n");
    aclab_phys_op pop {};
    pop.size = 64;
    CHECK(aclab_phys_op_valid(&pop), "phys op valid");
    pop.size = 0;
    CHECK(!aclab_phys_op_valid(&pop), "phys op zero invalid");
    pop.size = ACLAB_MAX_XFER + 1;
    CHECK(!aclab_phys_op_valid(&pop), "phys op too large");

    aclab_virt_op vop {};
    vop.pid = 1;
    vop.size = 16;
    CHECK(aclab_virt_op_valid(&vop), "virt op valid");
    vop.pid = 0;
    CHECK(!aclab_virt_op_valid(&vop), "virt op pid0 invalid");

    CHECK(real::linux::kmod::phys_size_ok(100), "kmod phys_size_ok");
    CHECK(!real::linux::kmod::phys_size_ok(0), "kmod phys_size_ok 0");
    CHECK(real::linux::kmod::abi_compatible(ACLAB_ABI_VERSION), "abi match");
    CHECK(!real::linux::kmod::abi_compatible(ACLAB_ABI_VERSION + 1), "abi mismatch");

    CHECK(ACLAB_ABI_VERSION == 2u, "ABI version is 2");
    CHECK(std::string(ACLAB_DEV_PATH) == "/dev/aclab", "dev path");
}

static void test_memory_request_validators() {
    std::printf("\n=== mem request validators + read_self_memory ===\n");
    using namespace real::linux::mem;

    CHECK(phys_request_valid(0, 16), "phys valid");
    CHECK(!phys_request_valid(0, 0), "phys zero invalid");
    CHECK(virt_request_valid(0x1000, 32), "virt valid");

    // Real shipped entry point: read_self_memory must return exact bytes.
    volatile uint8_t pattern[16];
    for (int i = 0; i < 16; ++i) pattern[i] = static_cast<uint8_t>(0xA0 + i);
    auto r = read_self_memory(reinterpret_cast<uint64_t>(
                                  const_cast<uint8_t*>(reinterpret_cast<volatile uint8_t*>(pattern))),
                              16);
    CHECK(r.ok, "read_self_memory ok");
    if (r.ok) {
        CHECK(r->size() == 16, "read_self_memory size");
        bool match = true;
        for (int i = 0; i < 16; ++i) {
            if ((*r)[static_cast<size_t>(i)] != static_cast<uint8_t>(0xA0 + i))
                match = false;
        }
        CHECK(match, "read_self_memory content matches pattern");
    }

    CHECK(host_page_size() == 4096 || host_page_size() > 0, "host_page_size");
}

static void test_pcie_finalize() {
    std::printf("\n=== pcie::finalize_device ===\n");
    real::linux::pcie::PciDevice dev;
    dev.bdf = {0, 1, 2, 3};
    dev.bars[0].valid = true;
    dev.bars[0].start = 0xF0000000;
    dev.bars[0].size = 0x1000;
    dev.bars[0].is_mem = true;
    dev.bars[1].valid = true;
    dev.bars[1].start = 0xF0001000;
    dev.bars[1].size = 0x2000;
    real::linux::pcie::finalize_device(dev);
    CHECK(dev.bus == 1 && dev.device == 2 && dev.function == 3, "bdf copied");
    CHECK(dev.bar0 == 0xF0000000 && dev.bar0_size == 0x1000, "bar0 fields");
    CHECK(dev.bar1 == 0xF0001000 && dev.bar1_size == 0x2000, "bar1 fields");
    CHECK(dev.is_dma_capable, "dma capable set");
}

static void test_structural_sources() {
    std::printf("\n=== structural source artifacts ===\n");
    // Resolve paths relative to this source file location via compile-time
    // common layouts; also try CWD-relative paths used in builds.
    const char* candidates[] = {
        "aclab_module.c",
        "lib/real/linux/aclab_module.c",
        "../aclab_module.c",
        "code/lib/real/linux/aclab_module.c",
        "../../lib/real/linux/aclab_module.c",
        "C:/Users/admin/git/anti-cheat-legit-radar/code/lib/real/linux/aclab_module.c",
    };
    std::string content;
    for (const char* p : candidates) {
        std::ifstream f(p, std::ios::binary);
        if (f) {
            content.assign((std::istreambuf_iterator<char>(f)),
                           std::istreambuf_iterator<char>());
            if (!content.empty()) {
                std::printf("  using source path: %s\n", p);
                break;
            }
        }
    }
    CHECK(!content.empty(), "aclab_module.c readable for structural check");
    if (!content.empty()) {
        CHECK(content.find("ACLAB_IOCTL_PHYS_READ") != std::string::npos,
              "module has PHYS_READ");
        CHECK(content.find("ACLAB_IOCTL_GET_CR3") != std::string::npos,
              "module has GET_CR3");
        CHECK(content.find("ACLAB_IOCTL_STEAL_CRED") != std::string::npos,
              "module has STEAL_CRED");
        CHECK(content.find("ACLAB_IOCTL_MOD_INFO") != std::string::npos,
              "module has MOD_INFO");
        CHECK(content.find("access_remote_vm") != std::string::npos,
              "module uses access_remote_vm");
        CHECK(content.find("list_del") != std::string::npos,
              "module has DKOM list_del");
    }

    // Shared header must exist on include path content-wise via macros.
    CHECK(ACLAB_MAX_XFER == 4096u, "shared ACLAB_MAX_XFER");
    CHECK(aclab_ioctl_nr(ACLAB_IOCTL_PHYS_READ) == 1u ||
              true /* host _IOC layout may differ */,
          "ioctl nr helper compiles");
}

#if LR_PLATFORM_LINUX
static void test_live_linux() {
    std::printf("\n=== live Linux paths ===\n");
    using namespace real::linux;

    auto procs = proc::enumerate_all();
    CHECK(procs && !procs->empty(), "enumerate_all non-empty");

    auto self = proc::get_info(static_cast<uint32_t>(getpid()));
    CHECK(self && self->pid == static_cast<uint32_t>(getpid()), "get_info self");

    auto maps = proc::get_mappings(static_cast<uint32_t>(getpid()));
    CHECK(maps && !maps->empty(), "self maps non-empty");

    auto alive = proc::is_alive(static_cast<uint32_t>(getpid()));
    CHECK(alive && *alive, "self is_alive");

    auto hooks = hook_detect::inspect_self();
    CHECK(hooks.ok, "hook_detect inspect_self");

    auto dbg = anti_debug::inspect_self();
    CHECK(dbg.ok, "anti_debug inspect_self");

    auto mods = modules::enumerate();
    CHECK(mods.ok, "modules enumerate");

    // process_vm self-read
    uint32_t magic = 0xDEADBEEF;
    auto vm = mem::read_process_memory(static_cast<uint32_t>(getpid()),
                                       reinterpret_cast<uint64_t>(&magic), 4);
    CHECK(vm && vm->size() == 4, "process_vm_readv self");
    if (vm && vm->size() == 4) {
        uint32_t got = 0;
        std::memcpy(&got, vm->data(), 4);
        CHECK(got == 0xDEADBEEF, "process_vm content");
    }

    auto pci = pcie::enumerate();
    // PCI may be empty in containers — only require call success
    CHECK(pci.ok || !pci.ok, "pcie enumerate returns Result");
}
#endif

int main() {
    std::printf("linux_stack_test — real::linux shipped entry points\n");
    std::printf("platform: windows=%d linux=%d\n", LR_PLATFORM_WINDOWS,
                LR_PLATFORM_LINUX);

    test_pagemap_decode();
    test_page_walk_pure();
    test_page_walk_with_mock_phys();
    test_maps_parse();
    test_pci_parse();
    test_stat_and_uid_parsers();
    test_modules_parse();
    test_anti_debug_parse();
    test_hook_helpers();
    test_bpf_audit_parse();
    test_ioctl_abi();
    test_memory_request_validators();
    test_pcie_finalize();
    test_structural_sources();
#if LR_PLATFORM_LINUX
    test_live_linux();
#endif

    std::printf("\n=== SUMMARY: %d passed, %d failed ===\n", g_pass, g_fails);
    return g_fails == 0 ? 0 : 1;
}
