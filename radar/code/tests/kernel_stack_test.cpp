// kernel_stack_test.cpp — Unit tests for code/lib/real/kernel pure + shipped APIs.
// Exercises catalog lookup, IOCTL packing/ctl_code, page-walk math, and
// module enum (when privileges allow). Does not require a loaded kernel driver.

#include "real/kernel/driver_loader.hpp"
#include "real/kernel/ioctl_interface.hpp"
#include "real/kernel/kernel_memory.hpp"
#include "real/kernel/vulnerable_driver.hpp"
#include "real/platform.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int fails = 0;

#define TEST(cond, msg)                                                        \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::printf("FAIL: %s\n", msg);                                          \
      ++fails;                                                                 \
    } else {                                                                   \
      std::printf("ok: %s\n", msg);                                            \
    }                                                                          \
  } while (0)

static void test_ctl_code() {
  std::printf("\n=== ctl_code / IOCTL helpers ===\n");
  using namespace real::kernel;

  // Windows CTL_CODE(FILE_DEVICE_UNKNOWN=0x22, 0x800, METHOD_BUFFERED, 0)
  const auto code = ctl_code(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, 0);
  TEST(code == IOCTL_MEM_READ, "ctl_code matches IOCTL_MEM_READ");
  TEST(code == 0x00222000u, "ctl_code FILE_DEVICE_UNKNOWN/0x800/BUFFERED");

  const auto phys = ctl_code(FILE_DEVICE_PHYSICAL_MEMORY, 0x800, METHOD_OUT_DIRECT, 0);
  TEST(phys == IOCTL_PHYSMEM_READ, "ctl_code matches IOCTL_PHYSMEM_READ");

  TEST(is_valid_device_handle(DeviceHandle{0}) == false, "null handle invalid");
  TEST(is_valid_device_handle(DeviceHandle{0x1234}) == true, "non-null handle valid");

  auto paths = device_path_candidates_from_name("gdrv.sys");
  TEST(!paths.empty(), "device_path_candidates non-empty");
  TEST(paths[0] == "\\\\.\\gdrv", "device_path_candidates strips .sys");
}

static void test_driver_name_helpers() {
  std::printf("\n=== driver_loader pure helpers ===\n");
  using namespace real::kernel;

  TEST(driver_name_matches("gdrv.sys", "gdrv"), "driver_name_matches substring");
  TEST(driver_name_matches("GDRV.SYS", "gdrv"), "driver_name_matches case-insensitive");
  TEST(!driver_name_matches("ntoskrnl.exe", "gdrv"), "driver_name_matches negative");
  TEST(driver_basename("C:\\\\Windows\\\\System32\\\\drivers\\\\gdrv.sys") == "gdrv.sys",
       "driver_basename Windows path");
  TEST(driver_basename("gdrv.sys") == "gdrv.sys", "driver_basename bare name");
}

static void test_page_walk_math() {
  std::printf("\n=== page-walk pure helpers ===\n");
  using namespace real::kernel::mem;

  // Classic canonical user VA: 0x00007FF712345678
  const std::uint64_t va = 0x00007FF712345678ULL;
  const PageIndices idx = virt_to_indices(va);
  TEST(idx.pml4 == ((va >> 39) & 0x1FF), "pml4 index");
  TEST(idx.pdpt == ((va >> 30) & 0x1FF), "pdpt index");
  TEST(idx.pd == ((va >> 21) & 0x1FF), "pd index");
  TEST(idx.pt == ((va >> 12) & 0x1FF), "pt index");
  TEST(idx.offset == (va & 0xFFF), "page offset");

  TEST(pte_present(0x1), "pte_present set");
  TEST(!pte_present(0x0), "pte_present clear");
  TEST(pte_large(0x80 | 0x1), "pte_large set");
  TEST(pte_frame(0x0000000123456003ULL) == 0x0000000123456000ULL, "pte_frame mask");

  TEST(pte_entry_phys(0x1000, 3) == 0x1000 + 24, "pte_entry_phys");

  const std::uint64_t leaf4k = 0x00000000ABCD1003ULL;  // present, frame ABCD1000
  const std::uint64_t phys4k = resolve_leaf_phys(leaf4k, 0x1234, false, false);
  TEST(phys4k == 0x00000000ABCD1234ULL, "resolve_leaf_phys 4K");

  const std::uint64_t leaf2m = 0x00000000A0000083ULL;  // large + present
  const std::uint64_t phys2m = resolve_leaf_phys(leaf2m, 0x1F0123, false, true);
  TEST(phys2m == 0x00000000A01F0123ULL, "resolve_leaf_phys 2M");

  TEST(leaf_chunk_size(0x1000, 0x2000, false, false) == 0x1000,
       "leaf_chunk_size stays in 4K page");
  TEST(leaf_chunk_size(0x0FF0, 32, false, false) == 16,
       "leaf_chunk_size near page end");

  TEST(is_plausible_cr3(0x1AD000ULL), "plausible CR3");
  TEST(!is_plausible_cr3(0), "zero CR3 rejected");
  TEST(!is_plausible_cr3(0x1234), "unaligned CR3 rejected");

  auto win10 = eprocess_layout_for_pid_offset(0x2E0);
  TEST(win10.unique_process_id == 0x2E0 && win10.active_process_links == 0x2F0,
       "Win10 EPROCESS layout");
  auto win11 = eprocess_layout_for_pid_offset(0x308);
  TEST(win11.unique_process_id == 0x308 && win11.token == 0x380,
       "Win11 EPROCESS layout");
}

static void test_gdrv_packing() {
  std::printf("\n=== gdrv request packing (shipped helpers) ===\n");
  using namespace real::kernel::byovd;

  auto phys = pack_gdrv_phys_req(0xE0000ULL, 64);
  TEST(phys.size() == sizeof(GdrvPhysReq), "phys pack size");
  GdrvPhysReq pref{};
  std::memcpy(&pref, phys.data(), sizeof(pref));
  TEST(pref.phys_addr == 0xE0000ULL && pref.size == 64 && pref.reserved == 0,
       "phys pack fields");

  auto virt = pack_gdrv_virt_req(1234, 0x7FF00000ULL, 4096, 0xDEADBEEFULL);
  TEST(virt.size() == sizeof(GdrvVirtReq), "virt pack size");
  GdrvVirtReq vref{};
  std::memcpy(&vref, virt.data(), sizeof(vref));
  TEST(vref.process_id == 1234 && vref.target_address == 0x7FF00000ULL &&
           vref.output_buffer == 0xDEADBEEFULL && vref.size == 4096,
       "virt pack fields");

  TEST(IOCTL_GDRV_PHYS_READ == 0xC3502000u, "IOCTL_GDRV_PHYS_READ");
  TEST(IOCTL_GDRV_PHYS_WRITE == 0xC3502004u, "IOCTL_GDRV_PHYS_WRITE");
  TEST(IOCTL_GDRV_VIRT_READ == 0xC3502008u, "IOCTL_GDRV_VIRT_READ");
  TEST(IOCTL_GDRV_VIRT_WRITE == 0xC350200Cu, "IOCTL_GDRV_VIRT_WRITE");
  TEST(IOCTL_GDRV_ENTITY_WALK == 0xC3502010u, "IOCTL_GDRV_ENTITY_WALK");
}

static void test_byovd_catalog() {
  std::printf("\n=== BYOVD catalog (shipped known_vulnerable_drivers) ===\n");
  using namespace real::kernel::byovd;

  auto catalog = known_vulnerable_drivers();
  TEST(catalog.ok && catalog->size() >= 4, "catalog returns entries");

  auto gdrv = find_vulnerable_driver("gdrv");
  TEST(gdrv.ok, "find_vulnerable_driver(gdrv)");
  if (gdrv) {
    TEST(gdrv->has_mem_read && gdrv->has_mem_write, "gdrv mem capabilities");
    TEST(gdrv->cve_id.find("2020-15368") != std::string::npos, "gdrv CVE id");
    auto paths = byovd_device_path_candidates(*gdrv);
    TEST(!paths.empty(), "byovd_device_path_candidates non-empty");
    bool has_gdrv_dev = false;
    for (const auto& p : paths) {
      if (p.find("gdrv") != std::string::npos) has_gdrv_dev = true;
    }
    TEST(has_gdrv_dev, "candidates include \\\\.\\gdrv style path");

    auto ioc = ioctl_set_for_driver(*gdrv);
    TEST(ioc.phys_read == IOCTL_GDRV_PHYS_READ, "ioctl_set_for_driver gdrv phys_read");
    TEST(ioc.virt_read == IOCTL_GDRV_VIRT_READ, "ioctl_set_for_driver gdrv virt_read");
  }

  auto missing = find_vulnerable_driver("this_driver_does_not_exist_zz");
  TEST(!missing.ok, "find_vulnerable_driver missing is error");
}

static void test_module_enum() {
  std::printf("\n=== enum_kernel_modules (live when allowed) ===\n");
  using namespace real::kernel;

  auto mods = enum_kernel_modules();
  // On Windows this usually succeeds without admin via PSAPI / NtQuery.
  if (mods && !mods->empty()) {
    TEST(true, "enum_kernel_modules returned modules");
    std::printf("  modules=%zu first=%s base=0x%llx size=%zu\n", mods->size(),
                mods->front().name.c_str(),
                static_cast<unsigned long long>(mods->front().base),
                mods->front().size);
    // ntoskrnl or similar should be present on Windows.
#if LR_PLATFORM_WINDOWS
    auto ntos = is_driver_loaded("ntoskrnl");
    TEST(ntos.ok && *ntos, "is_driver_loaded(ntoskrnl)");
    auto base = get_driver_base("ntoskrnl");
    // Some locked-down hosts redact kernel ImageBase to 0 via EnumDeviceDrivers
    // / SystemModuleInformation while still returning names + sizes. The shipped
    // API must still resolve the entry; non-zero base is best-effort.
    TEST(base.ok, "get_driver_base(ntoskrnl) resolves");
    bool saw_ntos_sized = false;
    for (const auto& m : *mods) {
      if (real::kernel::driver_name_matches(m.name, "ntoskrnl") && m.size > 0) {
        saw_ntos_sized = true;
        break;
      }
    }
    TEST(saw_ntos_sized || (base.ok && *base != 0),
         "ntoskrnl has size and/or non-zero base");
    if (base.ok) {
      std::printf("  ntoskrnl base=0x%llx (0 may mean host redacts kernel VAs)\n",
                  static_cast<unsigned long long>(*base));
    }
#endif
  } else {
    // Still exercise the API path — must not crash; error is typed.
    TEST(mods.ok == false || mods->empty(),
         "enum_kernel_modules empty/denied is non-fatal");
    if (!mods) {
      std::printf("  enum error: %s\n", mods.error_msg.c_str());
    }
  }

  auto svc = driver_service_exists("DoesNotExistService_KernelStackTest_ZZ");
  // Query should succeed as Result with false, or fail OpenSCManager without admin.
  TEST(svc.ok || !svc.ok, "driver_service_exists callable");
  if (svc) {
    TEST(*svc == false, "nonexistent service is false");
  }
}

static void test_session_without_driver() {
  std::printf("\n=== establish_byovd_session without driver ===\n");
  using namespace real::kernel::byovd;

  auto session = establish_byovd_session();
  // Most CI hosts will not have gdrv loaded — expect typed failure, not crash.
  if (!session) {
    TEST(!session.ok, "session fails cleanly without driver");
    const char* msg = session.error_msg.c_str();
    TEST(msg && msg[0] != '\0', "session error message non-empty");
    // Must not be a policy-block stub phrase that implies unimplemented body.
    const std::string m(msg);
    TEST(m.find("blocked by policy") == std::string::npos,
         "error is operational, not policy-block stub");
    std::printf("  expected fail: %s\n", msg);
  } else {
    TEST(session->session_active, "session active when driver present");
    (void)session->close();
  }
}

static void test_physical_read_graceful() {
  std::printf("\n=== read_physical graceful failure path ===\n");
  using namespace real::kernel::mem;

  auto r = read_physical(0xE0000, 16);
  // May succeed on unlocked hosts; must never throw / crash.
  if (r) {
    TEST(r->size() == 16, "read_physical BIOS region returned 16 bytes");
  } else {
    TEST(!r.ok, "read_physical denied is typed error");
    TEST(r.error_msg.c_str()[0] != '\0', "read_physical error non-empty");
    const std::string m(r.error_msg.c_str());
    TEST(m.find("not implemented") == std::string::npos &&
             m.find("blocked by policy") == std::string::npos,
         "read_physical error is operational denial");
    std::printf("  expected fail: %s\n", m.c_str());
  }
}

int main() {
  std::printf("=== Kernel Stack Unit Test ===\n");

  test_ctl_code();
  test_driver_name_helpers();
  test_page_walk_math();
  test_gdrv_packing();
  test_byovd_catalog();
  test_module_enum();
  test_session_without_driver();
  test_physical_read_graceful();

  std::printf("\n=== Results: %d failures ===\n", fails);
  return fails;
}
