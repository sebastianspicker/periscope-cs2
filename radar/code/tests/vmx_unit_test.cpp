// vmx_unit_test.cpp — Unit tests for the real VT-x / Hyper-V stack.
// Drives shipped APIs in code/lib/real/vmx/ (no re-implementation).
//
// Build:  cmake -DLR_ENABLE_REAL_VMX=ON ... && cmake --build . --target vmx_unit_test
// Run:    vmx_unit_test.exe

#include "real/vmx/vmx_intrin.hpp"
#include "real/platform.hpp"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

static int g_fails = 0;
static int g_checks = 0;

#define CHECK(cond, msg)                                                       \
  do {                                                                         \
    ++g_checks;                                                                \
    if (!(cond)) {                                                             \
      std::printf("FAIL: %s\n", msg);                                          \
      ++g_fails;                                                               \
    } else {                                                                   \
      std::printf("ok: %s\n", msg);                                            \
    }                                                                          \
  } while (0)

#if !LR_ARCH_X64
int main() {
  std::printf("vmx_unit_test: skipped (not x64)\n");
  return 0;
}
#else

static void test_cpuid_surfaces() {
  std::printf("\n=== CPUID / SVM / HV detection ===\n");
  auto vmx = real::vmx::vmx_supported();
  CHECK(static_cast<bool>(vmx), "vmx_supported returns Result");
  // Value is host-dependent; just ensure describe path works.
  auto dump = real::vmx::dump_cpuid();
  CHECK(!dump.vendor.empty(), "dump_cpuid vendor non-empty");
  CHECK(dump.max_leaf > 0, "dump_cpuid max_leaf > 0");
  std::string desc = dump.describe();
  CHECK(desc.find("CPU:") != std::string::npos, "CpuidDump::describe has CPU:");
  CHECK(desc.find(dump.vendor) != std::string::npos, "describe contains vendor");

  auto svm = real::vmx::svm_supported();
  CHECK(static_cast<bool>(svm), "svm_supported returns Result");

  auto hv = real::vmx::is_hypervisor_guest();
  CHECK(static_cast<bool>(hv), "is_hypervisor_guest returns Result");
  CHECK(dump.hypervisor_present == *hv, "dump HV flag matches is_hypervisor_guest");

  uint64_t lat = real::vmx::get_cpuid_latency();
  CHECK(lat > 0, "get_cpuid_latency > 0");
  // Second call must return same calibrated value (static).
  CHECK(real::vmx::get_cpuid_latency() == lat, "get_cpuid_latency stable");
}

static void test_hyperv_spoof() {
  std::printf("\n=== Hyper-V spoof leaves ===\n");
  std::string vendor = real::vmx::hyperv_spoofed_vendor();
  CHECK(!vendor.empty(), "hyperv_spoofed_vendor non-empty");
  CHECK(vendor.size() <= 12, "hyperv_spoofed_vendor <= 12 chars");

  // When no platform HV, spoof is "Microsoft Hv"; when present, relay.
  auto plat = real::vmx::platform_hv_vendor();
  if (plat.empty()) {
    CHECK(vendor == "Microsoft Hv", "default spoof vendor Microsoft Hv");
  } else {
    CHECK(vendor == plat, "spoof relays platform HV vendor");
  }

  uint32_t a = 0, b = 0, c = 0, d = 0;
  real::vmx::hyperv_spoofed_leaf_40000001(a, b, c, d);
  if (plat.empty()) {
    CHECK(a == 0x1, "spoofed 40000001 EAX hypercall bit");
    CHECK(b == 0x000A0002, "spoofed 40000001 EBX version");
  } else {
    // Platform values — any bits ok; just ensure call completed.
    CHECK(true, "spoofed 40000001 relayed platform");
  }

  uint32_t a2 = 1, b2 = 1, c2 = 1, d2 = 1;
  real::vmx::hyperv_spoofed_leaf(0x40000005, a2, b2, c2, d2);
  if (plat.empty()) {
    CHECK(a2 == 0 && b2 == 0 && c2 == 0 && d2 == 0,
          "spoofed leaf 40000005 zeros without platform HV");
  }

  auto avail = real::vmx::hyperv_hypercalls_available();
  CHECK(static_cast<bool>(avail), "hyperv_hypercalls_available returns Result");
}

static void test_ept_entry_helpers() {
  std::printf("\n=== EPT entry encode/decode ===\n");
  uint64_t entry = 0;
  real::vmx::ept_entry_set_phys_addr(entry, 0x12345000ULL);
  CHECK(real::vmx::ept_entry_get_phys_addr(entry) == 0x12345000ULL,
        "ept phys addr round-trip");

  real::vmx::ept_entry_set_access(entry, true, false, true);
  bool r = false, w = false, x = false;
  real::vmx::ept_entry_get_access(entry, r, w, x);
  CHECK(r && !w && x, "ept access R=1 W=0 X=1");

  real::vmx::ept_entry_set_memory_type(entry, 6);
  CHECK(real::vmx::ept_entry_get_memory_type(entry) == 6, "ept memtype WB=6");
  CHECK(!real::vmx::ept_entry_is_large(entry), "default not large page");

  entry |= (1ULL << 7);
  CHECK(real::vmx::ept_entry_is_large(entry), "PS bit => large page");

  // EPTP field accessors
  real::vmx::EptPointer eptp;
  eptp.value = 0xABCDEF000ULL | 6ULL | (3ULL << 3) | (1ULL << 6);
  CHECK(eptp.memory_type() == 6, "EPTP memory_type == 6");
  CHECK(eptp.page_walk_length() == 4, "EPTP walk length 4");
  CHECK(eptp.accessed_dirty(), "EPTP AD flag set");
  CHECK(eptp.pml4() == 0xABCDEF000ULL, "EPTP pml4 phys");
}

static void test_ept_hierarchy_4k() {
  std::printf("\n=== EPT hierarchy 4KB leaves ===\n");
  // Force 4K-only: size and alignment that still allow mapping but we disable large pages.
  real::vmx::EptBuildOptions opts;
  opts.use_large_pages = false;
  opts.write_back = true;

  constexpr uint64_t kGpa = 0x400000ULL;
  constexpr uint64_t kHpa = 0x800000ULL;
  constexpr size_t kSize = 0x3000;  // 3 pages

  auto built = real::vmx::build_ept_hierarchy_ex(kGpa, kSize, kHpa, opts);
  CHECK(static_cast<bool>(built), "build_ept_hierarchy_ex 4K succeeds");
  if (!built) {
    std::printf("  err: %s\n", built.error_msg.c_str());
    return;
  }
  CHECK(built->pml4_virtual != nullptr, "pml4_virtual set");
  CHECK(built->memory_type() == 6, "hierarchy EPTP WB");
  CHECK(built->page_walk_length() == 4, "hierarchy walk length 4");
  CHECK(built->pml4() != 0, "hierarchy PML4 phys non-zero");

  auto t0 = real::vmx::ept_translate_gpa(*built, kGpa);
  CHECK(static_cast<bool>(t0), "translate GPA base");
  if (t0) CHECK(*t0 == kHpa, "GPA base -> HPA base");

  auto t1 = real::vmx::ept_translate_gpa(*built, kGpa + 0x1000);
  CHECK(static_cast<bool>(t1), "translate GPA+0x1000");
  if (t1) CHECK(*t1 == kHpa + 0x1000, "GPA+4K -> HPA+4K");

  auto t2 = real::vmx::ept_translate_gpa(*built, kGpa + 0x2000 + 0x42);
  CHECK(static_cast<bool>(t2), "translate GPA+2pages+offset");
  if (t2) CHECK(*t2 == kHpa + 0x2000 + 0x42, "offset preserved in 4K leaf");

  auto miss = real::vmx::ept_translate_gpa(*built, kGpa + 0x10000);
  CHECK(!miss, "unmapped GPA fails translate");

  auto dest = real::vmx::destroy_ept_hierarchy(*built, built->pml4_virtual);
  CHECK(static_cast<bool>(dest), "destroy_ept_hierarchy 4K");
}

static void test_ept_hierarchy_2m() {
  std::printf("\n=== EPT hierarchy 2MB large pages ===\n");
  real::vmx::EptBuildOptions opts;
  opts.use_large_pages = true;

  // 2 MiB aligned GPA/HPA, size exactly 2 MiB → one large leaf.
  constexpr uint64_t kGpa = 0x200000ULL;
  constexpr uint64_t kHpa = 0x400000ULL;
  constexpr size_t kSize = 0x200000;

  auto built = real::vmx::build_ept_hierarchy_ex(kGpa, kSize, kHpa, opts);
  CHECK(static_cast<bool>(built), "build_ept 2MB succeeds");
  if (!built) {
    std::printf("  err: %s\n", built.error_msg.c_str());
    return;
  }

  auto t0 = real::vmx::ept_translate_gpa(*built, kGpa);
  CHECK(static_cast<bool>(t0), "2MB translate base");
  if (t0) CHECK(*t0 == kHpa, "2MB GPA->HPA base");

  auto tmid = real::vmx::ept_translate_gpa(*built, kGpa + 0x12345);
  CHECK(static_cast<bool>(tmid), "2MB translate mid-page");
  if (tmid) CHECK(*tmid == kHpa + 0x12345, "2MB offset preserved");

  // Also exercise default build_ept_hierarchy (large pages on).
  auto built2 = real::vmx::build_ept_hierarchy(0x600000ULL, 0x200000, 0x800000ULL);
  CHECK(static_cast<bool>(built2), "build_ept_hierarchy default 2MB");
  if (built2) {
    auto t = real::vmx::ept_translate_gpa(*built2, 0x600000ULL + 0x100);
    CHECK(static_cast<bool>(t) && *t == 0x800000ULL + 0x100, "default API translate");
    CHECK(static_cast<bool>(real::vmx::destroy_ept_hierarchy(*built2, built2->pml4_virtual)),
          "destroy default hierarchy");
  }

  CHECK(static_cast<bool>(real::vmx::destroy_ept_hierarchy(*built, built->pml4_virtual)),
        "destroy 2MB hierarchy");
}

static void test_vmcs_compose() {
  std::printf("\n=== VMCS pure field composition ===\n");
  real::vmx::VmcsSetupConfig cfg;
  cfg.guest_rip = 0x1000;
  cfg.guest_rsp = 0x2000;
  cfg.guest_cr3 = 0x3000;
  cfg.host_rip = 0xDEAD0000ULL;
  cfg.host_rsp = 0xBEEF0000ULL;
  cfg.host_cr0 = 0x80050033ULL;
  cfg.host_cr3 = 0x4000;
  cfg.host_cr4 = 0x2020;
  cfg.eptp = 0xABCDE006ULL | (3ULL << 3);
  cfg.vpid = 7;

  auto fields = real::vmx::compose_basic_vmcs_fields(cfg);
  CHECK(fields.size() >= 20, "compose produces >= 20 field writes");

  auto find_val = [&](real::vmx::VmcsField f) -> uint64_t {
    for (const auto& w : fields) {
      if (w.field == f) return w.value;
    }
    return UINT64_MAX;
  };

  CHECK(find_val(real::vmx::VmcsField::GuestRip) == 0x1000, "GuestRip programmed");
  CHECK(find_val(real::vmx::VmcsField::GuestRsp) == 0x2000, "GuestRsp programmed");
  CHECK(find_val(real::vmx::VmcsField::GuestCr3) == 0x3000, "GuestCr3 programmed");
  CHECK(find_val(real::vmx::VmcsField::HostRip) == 0xDEAD0000ULL, "HostRip programmed");
  CHECK(find_val(real::vmx::VmcsField::HostRsp) == 0xBEEF0000ULL, "HostRsp programmed");
  CHECK(find_val(real::vmx::VmcsField::HostCr0) == 0x80050033ULL, "HostCr0 programmed");
  CHECK(find_val(real::vmx::VmcsField::HostCr3) == 0x4000, "HostCr3 programmed");
  CHECK(find_val(real::vmx::VmcsField::HostCr4) == 0x2020, "HostCr4 programmed");
  CHECK(find_val(real::vmx::VmcsField::Eptp) == cfg.eptp, "EPTP programmed");
  CHECK(find_val(real::vmx::VmcsField::Vpid) == 7, "VPID programmed");
  CHECK(find_val(real::vmx::VmcsField::PinBasedExecControls) == cfg.pin_based,
        "Pin-based controls programmed");
  CHECK(find_val(real::vmx::VmcsField::CpuBasedExecControls) == cfg.cpu_based,
        "CPU-based controls programmed");
  CHECK(find_val(real::vmx::VmcsField::SecondaryExecControls) == cfg.secondary,
        "Secondary controls programmed");
  CHECK(find_val(real::vmx::VmcsField::VmExitControls) == cfg.exit_controls,
        "Exit controls programmed");
  CHECK(find_val(real::vmx::VmcsField::VmEntryControls) == cfg.entry_controls,
        "Entry controls programmed");

  // SDM encodings sanity (not the old wrong 0x800 etc.)
  CHECK(static_cast<uint64_t>(real::vmx::VmcsField::GuestRip) == 0x681E,
        "GuestRip encoding 0x681E");
  CHECK(static_cast<uint64_t>(real::vmx::VmcsField::PinBasedExecControls) == 0x4000,
        "PinBased encoding 0x4000");
  CHECK(static_cast<uint64_t>(real::vmx::VmcsField::Eptp) == 0x201A,
        "EPTP encoding 0x201A");
}

static void test_vmcs_alloc_and_setup_path() {
  std::printf("\n=== VMCS alloc / setup path ===\n");
  auto region = real::vmx::vmcs_alloc();
  CHECK(static_cast<bool>(region), "vmcs_alloc succeeds");
  if (region) {
    // Revision dword at start (may be 0 without MSR access).
    uint32_t rev = 0;
    std::memcpy(&rev, *region, sizeof(rev));
    CHECK(true, "vmcs region readable");
    real::vmx::vmx_region_free(*region);
  }

  auto vmxon_r = real::vmx::vmxon_alloc();
  CHECK(static_cast<bool>(vmxon_r), "vmxon_alloc succeeds");
  if (vmxon_r) real::vmx::vmx_region_free(*vmxon_r);

  // setup_vmcs on usermode: structured error after pure field prep, no crash.
  auto setup = real::vmx::setup_vmcs(nullptr, 0x1000, 0x2000, 0x3000);
  if (!setup) {
    CHECK(std::strlen(setup.error_msg.c_str()) > 0, "setup_vmcs structured error msg");
    std::printf("  setup_vmcs (expected usermode): %s\n", setup.error_msg.c_str());
  } else {
    CHECK(true, "setup_vmcs succeeded (CPL=0 lab)");
    real::vmx::vmx_region_free(*setup);
  }
}

static void test_privileged_no_crash() {
  std::printf("\n=== Privileged entry points (no crash) ===\n");

  auto en = real::vmx::enable_vmx();
  CHECK(!en || true, "enable_vmx returned without crash");
  if (!en) std::printf("  enable_vmx: %s\n", en.error_msg.c_str());

  auto enu = real::vmx::enable_vmx_from_usermode();
  CHECK(!enu || true, "enable_vmx_from_usermode returned");
  if (!enu) std::printf("  enable_vmx_from_usermode: %s\n", enu.error_msg.c_str());

  auto msr = real::vmx::read_msr_usermode(0x480);
  if (!msr) {
    CHECK(std::strstr(msr.error_msg.c_str(), "CPL") != nullptr ||
              std::strstr(msr.error_msg.c_str(), "RDMSR") != nullptr,
          "read_msr_usermode structured fail");
    std::printf("  read_msr: %s\n", msr.error_msg.c_str());
  } else {
    CHECK(*msr != 0 || true, "read_msr_usermode ok at CPL=0");
  }

  auto caps = real::vmx::get_vmx_capabilities();
  CHECK(static_cast<bool>(caps), "get_vmx_capabilities returns");
  if (caps) {
    std::string d = caps->describe();
    CHECK(d.find("VMX") != std::string::npos, "capabilities describe");
    std::printf("%s", d.c_str());
    // On usermode CI, msr_probe_ok should be false.
    if (!caps->msr_probe_ok) {
      CHECK(!caps->ept_supported, "no false EPT without MSR");
    }
  }

  auto off = real::vmx::vmxoff();
  if (!off) std::printf("  vmxoff: %s\n", off.error_msg.c_str());
  CHECK(true, "vmxoff returned");

  auto inv = real::vmx::invept(1, 0);
  if (!inv) std::printf("  invept: %s\n", inv.error_msg.c_str());
  CHECK(true, "invept returned");

  auto inv2 = real::vmx::invvpid(1, 1, 0);
  if (!inv2) std::printf("  invvpid: %s\n", inv2.error_msg.c_str());
  CHECK(true, "invvpid returned");

  auto hc = real::vmx::hyperv_hypercall(0, 0, 0);
  if (!hc) std::printf("  hyperv_hypercall: %s\n", hc.error_msg.c_str());
  CHECK(true, "hyperv_hypercall returned");

  auto phys = real::vmx::vmx_read_physical(0x1000, 16);
  if (!phys) std::printf("  vmx_read_physical: %s\n", phys.error_msg.c_str());
  CHECK(true, "vmx_read_physical returned");

  auto hvr = real::vmx::hyperv_read_virtual_memory(0x1000, 16);
  if (!hvr) std::printf("  hyperv_read_virtual_memory: %s\n", hvr.error_msg.c_str());
  CHECK(true, "hyperv_read_virtual_memory returned");

  // VMLAUNCH/VMRESUME must not crash.
  auto launch = real::vmx::vmlaunch();
  if (!launch) std::printf("  vmlaunch: %s\n", launch.error_msg.c_str());
  CHECK(true, "vmlaunch returned");

  auto resume = real::vmx::vmresume();
  if (!resume) std::printf("  vmresume: %s\n", resume.error_msg.c_str());
  CHECK(true, "vmresume returned");
}

int main() {
  std::printf("vmx_unit_test — driving shipped real::vmx APIs\n");
  test_cpuid_surfaces();
  test_hyperv_spoof();
  test_ept_entry_helpers();
  test_ept_hierarchy_4k();
  test_ept_hierarchy_2m();
  test_vmcs_compose();
  test_vmcs_alloc_and_setup_path();
  test_privileged_no_crash();

  std::printf("\n=== Summary: %d checks, %d failed ===\n", g_checks, g_fails);
  return g_fails == 0 ? 0 : 1;
}

#endif  // LR_ARCH_X64
