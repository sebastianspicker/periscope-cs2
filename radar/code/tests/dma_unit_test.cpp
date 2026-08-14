// dma_unit_test.cpp — Unit tests for the real DMA stack (page walk, scatter,
// backend lifecycle). Drives the shipped units in code/lib/real/dma/.
//
// Build:  cmake -DLR_ENABLE_REAL_DMA=ON ... && cmake --build . --target dma_unit_test
// Run:    dma_unit_test.exe

#include "real/dma/dma_backend.hpp"
#include "real/dma/page_walk.hpp"
#include "ac/types.hpp"

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <memory>
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

// ── Synthetic physical memory for page-walk tests ─────────────────
// Layout (all within 1 MiB):
//   CR3 / PML4 @ 0x1000
//   PDP        @ 0x2000
//   PD         @ 0x3000
//   PT         @ 0x4000
//   4K page    @ 0xA000   <- maps VA 0x00400000
//   2M page    @ 0x200000 <- maps VA 0x00800000 (PS in PDE)
//   1G page    @ 0x40000000 is not fully backed; we only need the entry
//               and reads of the low offset within our buffer — for 1G we
//               place a small backing region and only verify translation.

namespace {

constexpr uint64_t kCr3 = 0x1000;
constexpr uint64_t kPdp = 0x2000;
constexpr uint64_t kPd = 0x3000;
constexpr uint64_t kPt = 0x4000;
constexpr uint64_t kPage4kPa = 0xA000;
constexpr uint64_t kPage2mPa = 0x200000;

// VAs chosen so indices land in low PML4/PDP/PD/PT slots.
constexpr uint64_t kVa4k = 0x00400000ULL;  // PML4=0,PDP=0,PD=2,PT=0
constexpr uint64_t kVa2m = 0x00800000ULL;  // PML4=0,PDP=0,PD=4 (PS)
constexpr uint64_t kVa1g = 0x40000000ULL;  // PML4=0,PDP=1 (PS)

constexpr size_t kMemSize = 0x400000;  // 4 MiB synthetic phys

struct SynthMem {
  std::unique_ptr<uint8_t[]> mem;
  SynthMem() : mem(std::make_unique<uint8_t[]>(kMemSize)) {
    std::memset(mem.get(), 0, kMemSize);
    build();
  }

  void set_u64(uint64_t pa, uint64_t v) {
    if (pa + 8 <= kMemSize) std::memcpy(mem.get() + pa, &v, 8);
  }
  void set_bytes(uint64_t pa, const void* src, size_t n) {
    if (pa + n <= kMemSize) std::memcpy(mem.get() + pa, src, n);
  }

  void build() {
    using C = real::dma::PageTableConstants;
    // PML4[0] -> PDP
    set_u64(kCr3 + 0 * 8, kPdp | C::kPresent | 0x2);
    // PDP[0] -> PD (for 4K and 2M)
    set_u64(kPdp + 0 * 8, kPd | C::kPresent | 0x2);
    // PDP[1] -> 1GB page at PA 0x40000000 (PS). Translation only; phys read
    // of that PA will fail unless we alias — we only assert translate.
    set_u64(kPdp + 1 * 8, 0x40000000ULL | C::kPresent | C::kPageSizeBit | 0x2);

    // PD[2] -> PT for 4K mapping of VA 0x00400000
    // VA 0x00400000: PD index = (0x00400000 >> 21) & 0x1FF = 2
    set_u64(kPd + 2 * 8, kPt | C::kPresent | 0x2);
    // PT[0] -> 4K page
    set_u64(kPt + 0 * 8, kPage4kPa | C::kPresent | 0x2);

    // PD[4] -> 2MB page at 0x200000 for VA 0x00800000
    // VA 0x00800000: PD index = (0x00800000 >> 21) & 0x1FF = 4
    set_u64(kPd + 4 * 8, kPage2mPa | C::kPresent | C::kPageSizeBit | 0x2);

    // Payload in 4K page
    const char payload[] = "DMA4KOK!";
    set_bytes(kPage4kPa, payload, sizeof(payload));
    // Payload at offset inside 2M page
    const char payload2[] = "DMA2MOK!";
    set_bytes(kPage2mPa + 0x1234, payload2, sizeof(payload2));
  }

  bool read(uint64_t pa, void* buf, size_t sz) const {
    if (pa + sz > kMemSize) return false;
    std::memcpy(buf, mem.get() + pa, sz);
    return true;
  }

  static bool phys_cb(uint64_t pa, void* buf, size_t sz, void* ctx) {
    return static_cast<SynthMem*>(ctx)->read(pa, buf, sz);
  }
};

}  // namespace

static void test_page_walk_4k() {
  std::printf("\n=== page walk 4KB ===\n");
  SynthMem mem;
  auto r = real::dma::translate_va(kVa4k, kCr3, SynthMem::phys_cb, &mem);
  CHECK(r.present, "4KB mapping present");
  CHECK(r.page_size == real::dma::PageTableConstants::kPage4k, "4KB page size");
  CHECK(r.phys_addr == kPage4kPa, "4KB VA -> expected PA");

  auto r_off =
      real::dma::translate_va(kVa4k + 0x42, kCr3, SynthMem::phys_cb, &mem);
  CHECK(r_off.present && r_off.phys_addr == kPage4kPa + 0x42,
        "4KB offset preserved");
}

static void test_page_walk_2m() {
  std::printf("\n=== page walk 2MB ===\n");
  SynthMem mem;
  auto r = real::dma::translate_va(kVa2m, kCr3, SynthMem::phys_cb, &mem);
  CHECK(r.present, "2MB mapping present");
  CHECK(r.page_size == real::dma::PageTableConstants::kPage2m, "2MB page size");
  CHECK(r.phys_addr == kPage2mPa, "2MB VA -> expected PA");

  auto r_off =
      real::dma::translate_va(kVa2m + 0x1234, kCr3, SynthMem::phys_cb, &mem);
  CHECK(r_off.present && r_off.phys_addr == kPage2mPa + 0x1234,
        "2MB offset preserved");
}

static void test_page_walk_1g() {
  std::printf("\n=== page walk 1GB ===\n");
  SynthMem mem;
  auto r = real::dma::translate_va(kVa1g, kCr3, SynthMem::phys_cb, &mem);
  CHECK(r.present, "1GB mapping present");
  CHECK(r.page_size == real::dma::PageTableConstants::kPage1g, "1GB page size");
  CHECK(r.phys_addr == 0x40000000ULL, "1GB VA -> expected PA");

  auto r_off =
      real::dma::translate_va(kVa1g + 0xABC, kCr3, SynthMem::phys_cb, &mem);
  CHECK(r_off.present && r_off.phys_addr == 0x40000000ULL + 0xABC,
        "1GB offset preserved");
}

static void test_page_walk_unmapped() {
  std::printf("\n=== page walk unmapped ===\n");
  SynthMem mem;
  auto r = real::dma::translate_va(0x0, kCr3, SynthMem::phys_cb, &mem);
  // VA 0: PD index 0, PT not present
  CHECK(!r.present && r.phys_addr == 0, "unmapped VA (0) rejected");

  r = real::dma::translate_va(0x7F0000000000ULL, kCr3, SynthMem::phys_cb, &mem);
  CHECK(!r.present, "unmapped high VA rejected");

  r = real::dma::translate_va(kVa4k, kCr3, nullptr, &mem);
  CHECK(!r.present, "null PhysReadFn rejected");

  CHECK(real::dma::translate_va_or_zero(kVa4k, kCr3, SynthMem::phys_cb, &mem) ==
            kPage4kPa,
        "translate_va_or_zero success");
  CHECK(real::dma::translate_va_or_zero(0x0, kCr3, SynthMem::phys_cb, &mem) == 0,
        "translate_va_or_zero unmapped");
}

static void test_read_virtual() {
  std::printf("\n=== read_virtual multi-page ===\n");
  SynthMem mem;

  char buf[16] = {};
  bool ok = real::dma::read_virtual(kVa4k, buf, 8, kCr3, SynthMem::phys_cb, &mem);
  CHECK(ok, "read_virtual 4K page");
  CHECK(std::memcmp(buf, "DMA4KOK!", 8) == 0, "read_virtual 4K payload");

  char buf2[16] = {};
  ok = real::dma::read_virtual(kVa2m + 0x1234, buf2, 8, kCr3, SynthMem::phys_cb,
                               &mem);
  CHECK(ok, "read_virtual 2M page");
  CHECK(std::memcmp(buf2, "DMA2MOK!", 8) == 0, "read_virtual 2M payload");

  // Cross-page: need a second 4K mapping. Map PT[1] -> 0xB000 and put data
  // straddling the boundary.
  using C = real::dma::PageTableConstants;
  mem.set_u64(kPt + 1 * 8, 0xB000 | C::kPresent | 0x2);
  uint8_t tail[4] = {0xAA, 0xBB, 0xCC, 0xDD};
  uint8_t head[4] = {0x11, 0x22, 0x33, 0x44};
  mem.set_bytes(kPage4kPa + 0xFFC, tail, 4);
  mem.set_bytes(0xB000, head, 4);

  uint8_t crossed[8] = {};
  ok = real::dma::read_virtual(kVa4k + 0xFFC, crossed, 8, kCr3, SynthMem::phys_cb,
                               &mem);
  CHECK(ok, "read_virtual crosses 4K boundary");
  CHECK(crossed[0] == 0xAA && crossed[3] == 0xDD && crossed[4] == 0x11 &&
            crossed[7] == 0x44,
        "cross-page bytes correct");

  uint8_t dummy = 0;
  ok = real::dma::read_virtual(0x7F0000000000ULL, &dummy, 1, kCr3,
                               SynthMem::phys_cb, &mem);
  CHECK(!ok, "read_virtual unmapped fails");

  auto bytes =
      real::dma::read_virtual_bytes(kVa4k, 8, kCr3, SynthMem::phys_cb, &mem);
  CHECK(bytes && bytes->size() == 8, "read_virtual_bytes Result ok");
}

static void test_scatter_list() {
  std::printf("\n=== scatter-gather descriptors ===\n");

  // Aligned single page
  auto d1 = real::dma::build_scatter_list(0x1000, 0x1000, 0x1000);
  CHECK(d1.size() == 1, "aligned 4K -> 1 descriptor");
  CHECK(d1[0].src_phys == 0x1000 && d1[0].length == 0x1000 && d1[0].flags == 1,
        "aligned 4K descriptor fields");

  // Unaligned: start 0xF00, size 0x2200 → ends at 0x3100.
  // Chunks: [0xF00,0x100) + [0x1000,0x1000) + [0x2000,0x1000) + [0x3000,0x100)
  auto d2 = real::dma::build_scatter_list(0xF00, 0x2200, 0x1000);
  CHECK(d2.size() == 4, "unaligned multi-page -> 4 descriptors");
  CHECK(d2[0].src_phys == 0xF00 && d2[0].length == 0x100, "first partial page");
  CHECK(d2[1].src_phys == 0x1000 && d2[1].length == 0x1000, "middle full page");
  CHECK(d2[2].src_phys == 0x2000 && d2[2].length == 0x1000, "second full page");
  CHECK(d2[3].src_phys == 0x3000 && d2[3].length == 0x100, "last partial page");
  // Total length
  uint32_t total = 0;
  for (auto& d : d2) total += d.length;
  CHECK(total == 0x2200, "scatter total length matches");

  // Empty
  auto d0 = real::dma::build_scatter_list(0, 0, 0x1000);
  CHECK(d0.empty(), "size 0 -> empty list");

  // Large multi-page
  auto d3 = real::dma::build_scatter_list(0x0, 0x5000, 0x1000);
  CHECK(d3.size() == 5, "5 full pages");
  for (size_t i = 0; i < d3.size(); ++i) {
    CHECK(d3[i].dst_offset == i * 0x1000, "dst_offset sequential");
  }

  // Virtual scatter via page walk
  SynthMem mem;
  auto vs = real::dma::build_virtual_scatter(kVa4k, 0x20, kCr3, SynthMem::phys_cb,
                                            &mem, 0x1000);
  CHECK(vs && vs->size() == 1, "virtual scatter single page");
  CHECK((*vs)[0].src_phys == kPage4kPa && (*vs)[0].length == 0x20,
        "virtual scatter phys + len");

  auto vs_fail = real::dma::build_virtual_scatter(0x7F0000000000ULL, 16, kCr3,
                                                 SynthMem::phys_cb, &mem);
  CHECK(!vs_fail, "virtual scatter unmapped fails");
}

static void test_backend_lifecycle() {
  std::printf("\n=== RealDmaBackend lifecycle ===\n");

  real::dma::RealDmaBackend backend(false);
  CHECK(!backend.is_attached(), "starts detached");
  CHECK(backend.name() == "real_dma", "name() is real_dma");
  CHECK(backend.active_transport() == real::dma::RealDmaBackend::Transport::None,
        "transport None initially");

  auto status = backend.attach(0);
  // Without privileged hardware the expected result is Unavailable.
  // If the host actually allows physmem, Ok is also valid.
  CHECK(status == ac::Status::Unavailable || status == ac::Status::Ok,
        "attach returns Unavailable or Ok");
  if (status == ac::Status::Unavailable) {
    CHECK(!backend.is_attached(), "not attached after failed attach");
  } else {
    CHECK(backend.is_attached(), "attached after successful attach");
  }

  // Read while detached (or after failed attach) must not crash.
  if (!backend.is_attached()) {
    ac::ReadRequest req{0x1000, 16};
    auto rr = backend.read(req);
    CHECK(rr.status == ac::Status::Denied, "read denied when detached");
  } else {
    ac::ReadRequest req{0x1000, 16};
    auto rr = backend.read(req);
    CHECK(rr.status == ac::Status::Ok || rr.status == ac::Status::Denied,
          "read returns Ok or Denied when attached");
    backend.detach();
    CHECK(!backend.is_attached(), "detach clears attached");
  }

  // Detach is always safe (idempotent).
  backend.detach();
  backend.detach();
  CHECK(!backend.is_attached(), "double detach safe");

  // FPGA-preferring backend also fails soft without device.
  real::dma::RealDmaBackend fpga_backend(true);
  auto st2 = fpga_backend.attach(1);
  CHECK(st2 == ac::Status::Unavailable || st2 == ac::Status::Ok,
        "FPGA backend attach soft-fails without device");
  fpga_backend.detach();
  CHECK(!fpga_backend.is_attached(), "FPGA backend detach clean");
}

// real::Result contract used by all DMA bool APIs:
//   Result(true)           → ok=true,  value=true   (success affirmative)
//   Result(false, "msg")   → ok=false, value=false  (structured failure)
//   Result(true, "msg")    → ok=false, value=true   ← BUG: error ctor
// Therefore any value==true with ok==false is a construction bug.
template <typename T>
static void check_result_success_ctor(const real::Result<T>& r, const char* api) {
  // value-true must never come from the error constructor.
  char buf[160];
  std::snprintf(buf, sizeof(buf),
                "%s: value==true implies ok==true (no Result(T,msg) success)",
                api);
  CHECK(!(static_cast<bool>(r.value) && !r.ok), buf);
  if (r.ok) {
    std::snprintf(buf, sizeof(buf), "%s: ok Result has empty error_msg", api);
    CHECK(r.error_msg.empty(), buf);
  } else {
    std::snprintf(buf, sizeof(buf),
                  "%s: failed Result has non-empty error_msg", api);
    CHECK(!r.error_msg.empty(), buf);
  }
}

static void test_result_ctor_contract() {
  std::printf("\n=== Result constructor contract (real::Result) ===\n");
  // Drive the same Result type the DMA stack returns. These assertions
  // lock the ctor semantics so Result(true,"msg") can never be "success".
  real::Result<bool> ok_true(true);
  CHECK(ok_true.ok && ok_true.value && ok_true.error_msg.empty(),
        "Result(true) is success with value true");

  real::Result<bool> err_false(false, "absent");
  CHECK(!err_false.ok && !err_false.value && !err_false.error_msg.empty(),
        "Result(false,msg) is failure with value false");

  // The bug pattern: two-arg ctor always sets ok=false regardless of value.
  real::Result<bool> bug_true(true, "should not be success");
  CHECK(!bug_true.ok && bug_true.value,
        "Result(true,msg) has ok=false (error ctor) — DMA must not use this for success");
  // And the invariant check_result_success_ctor would flag it:
  CHECK(static_cast<bool>(bug_true.value) && !bug_true.ok,
        "detector sees value=true with ok=false as the bad pattern");
}

static void test_public_api_soft_fail() {
  std::printf("\n=== public DMA APIs soft-fail + Result contract ===\n");

  auto iommu = real::dma::iommu_enabled();
  check_result_success_ctor(iommu, "iommu_enabled");
  // If the host reports IOMMU on, Result must be operator-bool true.
  if (iommu.value) {
    CHECK(iommu.ok && static_cast<bool>(iommu),
          "iommu_enabled true value is operator-bool success");
  } else {
    CHECK(!iommu.ok, "iommu_enabled absent is structured failure");
  }

  auto bypass = real::dma::iommu_bypass();
  check_result_success_ctor(bypass, "iommu_bypass");
  if (bypass.value) {
    // ACS observed/cleared — must be Result(true), not Result(true, msg).
    CHECK(bypass.ok && static_cast<bool>(bypass) && *bypass,
          "iommu_bypass success is Result(true)");
  } else {
    CHECK(!bypass.ok && !bypass.error_msg.empty(),
          "iommu_bypass failure has error message");
  }

  auto tb = real::dma::thunderbolt_available();
  check_result_success_ctor(tb, "thunderbolt_available");
  if (tb.value) {
    CHECK(tb.ok && static_cast<bool>(tb),
          "thunderbolt_available true is success");
  } else {
    CHECK(!tb.ok, "thunderbolt_available absent is structured failure");
  }

  auto dfu = real::dma::usb_dfu_dma_possible();
  check_result_success_ctor(dfu, "usb_dfu_dma_possible");
  if (dfu.value) {
    // Would have failed under Result(true, "USB DFU present") bug.
    CHECK(dfu.ok && static_cast<bool>(dfu) && *dfu,
          "usb_dfu_dma_possible present is Result(true)");
  } else {
    CHECK(!dfu.ok && !dfu.error_msg.empty(),
          "usb_dfu_dma_possible absent is structured failure");
  }

  auto pm = real::dma::physmem_read(0x1000, 16);
  if (pm.ok) {
    CHECK(pm.error_msg.empty(), "physmem_read success has empty error");
    CHECK(pm->size() == 16 || pm->size() > 0,
          "physmem_read success returns bytes");
  } else {
    CHECK(!pm.error_msg.empty(), "physmem_read failure message non-empty");
  }

  auto smi = real::dma::acpi_smi_read(0x1000, 16);
  // Falls back to physmem; either success buffer or structured failure.
  if (smi.ok) {
    CHECK(smi.error_msg.empty(), "acpi_smi_read success empty error");
  } else {
    CHECK(!smi.error_msg.empty(), "acpi_smi_read failure message non-empty");
  }

  auto devices = real::dma::enum_pci_devices();
  if (devices.ok) {
    CHECK(devices.error_msg.empty(), "enum_pci_devices success empty error");
    std::printf("  enumerated %zu PCI devices\n", devices->size());
  } else {
    CHECK(!devices.error_msg.empty(),
          "enum_pci_devices failure message non-empty");
  }

  auto fpga = real::dma::fpga_dma_open(
#if LR_PLATFORM_WINDOWS
      "\\\\.\\DoesNotExistFpga"
#else
      "/dev/does_not_exist_fpga"
#endif
  );
  CHECK(!fpga.ok, "fpga_dma_open missing device fails (ok=false)");
  CHECK(!fpga.error_msg.empty(), "fpga_dma_open failure message non-empty");
  {
    real::dma::FpgaDmaDevice empty;
    auto c = real::dma::fpga_dma_close(empty);
    CHECK(c.ok, "fpga_dma_close on closed device ok");
  }
}

int main() {
  std::printf("=== DMA unit tests (shipped real::dma) ===\n");

  test_result_ctor_contract();
  test_page_walk_4k();
  test_page_walk_2m();
  test_page_walk_1g();
  test_page_walk_unmapped();
  test_read_virtual();
  test_scatter_list();
  test_backend_lifecycle();
  test_public_api_soft_fail();

  std::printf("\n=== Results: %d checks, %d failures ===\n", g_checks, g_fails);
  return g_fails == 0 ? 0 : 1;
}
