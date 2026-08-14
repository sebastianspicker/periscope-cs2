// dma_radar.cpp — T4 red DMA/off-box path on World.trust.dma_* and structural fog.
// Simulated PCIe/DMA hardware residuals.

#include "t4_red/dma_radar.hpp"

#include <algorithm>
#include <cstring>
#include <sstream>

namespace t4_red {

namespace {

constexpr std::size_t kDmaPageSize = 0x1000;
constexpr std::size_t kEntityTableHeaderSize = 0x10;
constexpr std::uint32_t kMaxEntityCount = 64;

struct RawEntity {
  float x;
  float y;
  float z;
  std::uint8_t team;
  std::uint8_t alive;
  std::uint8_t padding[2];
};

}  // namespace

// T4 educational DMA residual client on sim::World only.
// Off-box path: dma_device_present, iommu off, dma_read without local reader.

DmaRadar::DmaRadar(sim::World& world) : world_(world) {}

// DmaRadar::enable_hardware_path: Enable sim DMA hardware path on World.trust.
void DmaRadar::enable_hardware_path() {
  world_.trust.dma_device_present = true;
  world_.trust.iommu_on = false;
  enabled_ = true;
  world_.note("t4 DmaRadar enable_hardware_path dma=1 iommu=off");
}

// DmaRadar::probe_iommu_blocks: True if IOMMU policy would block DMA probe.
bool DmaRadar::probe_iommu_blocks() {
  // With IOMMU on, dma_read must fail in the lab; then restore off for residual.
  const bool prev = world_.trust.iommu_on;
  world_.trust.iommu_on = true;
  const auto game = world_.game_pid();
  std::vector<std::uint8_t> buf;
  bool blocked = true;
  if (game) {
    if (auto* g = world_.proc(game)) {
      blocked = !world_.dma_read(game, g->base, 4, buf);
    }
  }
  world_.trust.iommu_on = false;  // residual path prefers iommu off
  last_.iommu_blocked_probe = blocked;
  return blocked;
}

// DmaRadar::enable_fpga_smart_dma: Enable the World-only FPGA scatter read scar.
void DmaRadar::enable_fpga_smart_dma() {
  world_.trust.dma_device_present = true;
  world_.fpga_smart_dma_active = true;
  world_.note("t4 DmaRadar fpga_smart_dma scatter_gather=1 sim_only=1");
}

// DmaRadar::run_scatter_read: Read the requested simulated memory one page at a time.
bool DmaRadar::run_scatter_read(std::uint32_t target_pid, std::uint64_t address,
                                std::size_t size,
                                std::vector<std::uint8_t>& out) {
  out.clear();
  if (!world_.fpga_smart_dma_active || size == 0) {
    return false;
  }

  out.reserve(size);
  for (std::size_t offset = 0; offset < size; offset += kDmaPageSize) {
    const auto chunk_size = std::min(kDmaPageSize, size - offset);
    std::vector<std::uint8_t> page;
    if (!world_.dma_read(target_pid, address + offset, chunk_size, page)) {
      out.clear();
      return false;
    }
    out.insert(out.end(), page.begin(), page.end());
    ++world_.fpga_scatter_reads;
    ++dma_ops_;
    bytes_read_ += page.size();
    if (world_.pcie_peer_dma_active) {
      ++world_.pcie_peer_transactions;
    }
  }
  world_.note("t4 DmaRadar scatter_read pages=" +
              std::to_string(world_.fpga_scatter_reads));
  return out.size() == size;
}

// DmaRadar::hidden_rescan_mode: Record low-rate re-scanning in the World narrative.
void DmaRadar::hidden_rescan_mode() {
  world_.fpga_hidden_rescan = true;
  world_.note("t4 DmaRadar fpga_hidden_rescan sim_only=1");
}

// DmaRadar::enable_iommu_bypass: Activate only the World model's ATS/ACS bypass scar.
void DmaRadar::enable_iommu_bypass() {
  world_.trust.dma_device_present = true;
  world_.iommu_bypass_active = true;
  world_.iommu_bypass_confirmed = true;
  world_.note("t4 DmaRadar iommu_bypass ATS_ACS simulated confirmed=1");
}

// DmaRadar::enable_pcie_peer_dma: Model source-to-exfil-device peer DMA transfers.
void DmaRadar::enable_pcie_peer_dma() {
  world_.trust.dma_device_present = true;
  world_.pcie_peer_dma_active = true;
  world_.pcie_peer_bypassed_iommu =
      world_.iommu_bypass_active && world_.iommu_bypass_confirmed;
  world_.note("t4 DmaRadar pcie_peer_dma source_device=1 exfil_device=1 sim_only=1");
}

// DmaRadar::enable_thunderbolt_dma: Model Thunderbolt hotplug DMA in World only.
void DmaRadar::enable_thunderbolt_dma() {
  world_.thunderbolt_dma_active = true;
  world_.thunderbolt_security_bypassed = true;
  last_.thunderbolt_dma = true;
  last_.thunderbolt_security_bypassed = true;
  world_.note("t4 DmaRadar thunderbolt_dma security_bypassed=1 sim_only=1");
}

// DmaRadar::enable_pcie_bar_mmio: Model PCIe BAR physical-memory remapping.
void DmaRadar::enable_pcie_bar_mmio(int count) {
  world_.pcie_bar_mmio_active = true;
  world_.pcie_bar_remap_count = count;
  last_.pcie_bar_mmio = true;
  last_.pcie_bar_remap_count = count;
  world_.note("t4 DmaRadar pcie_bar_mmio remaps=" + std::to_string(count) +
              " sim_only=1");
}

// DmaRadar::enable_acpi_dsdt_override: Model firmware persistence in World only.
void DmaRadar::enable_acpi_dsdt_override() {
  world_.acpi_dsdt_override_active = true;
  last_.acpi_dsdt_override = true;
  world_.note("t4 DmaRadar acpi_dsdt_override sim_only=1");
}

// DmaRadar::enable_usb_dfu_dma: Model USB DFU DMA with a lab device identity.
void DmaRadar::enable_usb_dfu_dma(const std::string& device_id) {
  world_.usb_dfu_dma_active = true;
  world_.usb_dfu_device_id = device_id;
  last_.usb_dfu_dma = true;
  world_.note("t4 DmaRadar usb_dfu_dma device=" + device_id + " sim_only=1");
}

// DmaRadar::pull_entities: Refresh entity pipeline and cache living set for radar UI.
bool DmaRadar::pull_entities() {
  entities_.clear();
  const auto game = world_.game_pid();
  if (!game) {
    last_.detail = "no_game";
    return false;
  }
  auto* g = world_.proc(game);
  if (!g) {
    return false;
  }
  if (g->memory.size() < 0x30) {
    world_.plant_lab_entities(game);
  }
  // Parse lab entity table: first uint32 count, then entities.
  if (g->memory.size() < 4) {
    last_.detail = "empty_memory";
    return false;
  }
  std::vector<std::uint8_t> buf;
  const bool using_scatter = world_.fpga_smart_dma_active;
  bool ok = false;
  if (using_scatter) {
    ok = run_scatter_read(game, g->base, g->memory.size(), buf);
  } else {
    ok = world_.dma_read(game, g->base, g->memory.size(), buf);
  }
  if (ok && !using_scatter) {
    ++dma_ops_;
    bytes_read_ += buf.size();
    if (world_.pcie_peer_dma_active) {
      ++world_.pcie_peer_transactions;
    }
  }
  if (ok && buf.size() >= sizeof(std::uint32_t)) {
    std::uint32_t count = 0;
    std::memcpy(&count, buf.data(), sizeof(count));
    count = std::min(count, kMaxEntityCount);
    for (std::uint32_t i = 0; i < count; ++i) {
      const auto offset = kEntityTableHeaderSize + i * sizeof(RawEntity);
      if (offset + sizeof(RawEntity) > buf.size()) {
        break;
      }
      RawEntity raw{};
      std::memcpy(&raw, buf.data() + offset, sizeof(raw));
      entities_.push_back(
          ac::EntitySnapshot{i + 1, {raw.x, raw.y, raw.z}, raw.team,
                             raw.alive != 0});
    }
  }
  last_.offbox_read_ok = ok;
  last_.entities_ok = !entities_.empty();
  last_.entity_count = static_cast<int>(entities_.size());
  last_.dma_ops = dma_ops_;
  last_.bytes_read = bytes_read_;
  last_.fpga_active = world_.fpga_smart_dma_active;
  last_.fpga_scatter_count = world_.fpga_scatter_reads;
  last_.fpga_hidden_rescan = world_.fpga_hidden_rescan;
  last_.iommu_bypass = world_.iommu_bypass_active;
  last_.iommu_bypass_confirmed = world_.iommu_bypass_confirmed;
  last_.pcie_peer_dma = world_.pcie_peer_dma_active;
  last_.pcie_peer_txns = world_.pcie_peer_transactions;
  last_.pcie_peer_bypass_ok = world_.pcie_peer_bypassed_iommu;
  last_.thunderbolt_dma = world_.thunderbolt_dma_active;
  last_.thunderbolt_security_bypassed = world_.thunderbolt_security_bypassed;
  last_.pcie_bar_mmio = world_.pcie_bar_mmio_active;
  last_.pcie_bar_remap_count = world_.pcie_bar_remap_count;
  last_.acpi_dsdt_override = world_.acpi_dsdt_override_active;
  last_.usb_dfu_dma = world_.usb_dfu_dma_active;
  return last_.entities_ok;
}

// DmaRadar::apply_residuals: Plant DMA residual scars (IOMMU off, dual-boot, etc.).
void DmaRadar::apply_residuals(bool capture, bool desktop, bool clone, bool lag,
                               bool dual_boot) {
  if (capture) {
    world_.trust.capture_card_present = true;
    last_.capture_residual = true;
  }
  if (desktop) {
    world_.desktop_duplication = true;
    last_.desktop_dup = true;
  }
  if (clone) {
    world_.external_display_clone = true;
    last_.external_clone = true;
  }
  if (lag) {
    world_.lag_switch_active = true;
    last_.lag_switch = true;
  }
  if (dual_boot) {
    world_.trust.dual_boot_profile = true;
    last_.dual_boot = true;
  }
  world_.note("t4 DmaRadar apply_residuals");
}

// DmaRadar::apply_all_residuals: Plant all T4 off-box residual scars at once.
void DmaRadar::apply_all_residuals() {
  apply_residuals(true, true, true, true, true);
  world_.clipcursor_confined = true;
  world_.packet_loss_faked = true;
  last_.clipcursor = true;
  last_.packet_loss_faked = true;
  world_.note("t4 DmaRadar apply_all_residuals clipcursor=1 packet_loss=1");
}

// DmaRadar::process_list_is_clean: True when no local reader process scar (off-box story).
bool DmaRadar::process_list_is_clean() const {
  const auto game = world_.game_pid();
  for (const auto& h : world_.handles_to(game)) {
    if (!sim::has(h.access, sim::AccessMask::VmRead)) {
      continue;
    }
    const auto* p = world_.proc(h.owner_pid);
    if (p && !p->is_game && !p->is_ac) {
      return false;
    }
  }
  return true;
}

// DmaRadar::hardware_enabled: Whether DMA hardware path is currently enabled.
bool DmaRadar::hardware_enabled() const { return enabled_; }

// DmaRadar::run_full_loop: Red attach/read then blue sensors/mitigate on one World tick.
DmaRadarReport DmaRadar::run_full_loop(bool residuals) {
  last_ = {};
  enable_hardware_path();
  last_.hardware_enabled = enabled_;
  last_.iommu_off = !world_.trust.iommu_on;
  last_.iommu_blocked_probe = probe_iommu_blocks();
  pull_entities();
  last_.process_list_clean = process_list_is_clean();
  if (residuals) {
    apply_residuals(true, true, true, false, false);
  }
  std::ostringstream oss;
  oss << "dma_full entities=" << last_.entity_count
      << " clean=" << (last_.process_list_clean ? 1 : 0)
      << " offbox=" << (last_.offbox_read_ok ? 1 : 0)
      << " ops=" << last_.dma_ops;
  last_.detail = oss.str();
  world_.note("t4 DmaRadar " + last_.detail);
  return last_;
}

// DmaRadar::run_full_stealth_loop: Exercise every simulated T4 off-box scar.
DmaRadarReport DmaRadar::run_full_stealth_loop() {
  last_ = {};
  enable_hardware_path();
  enable_fpga_smart_dma();
  hidden_rescan_mode();
  enable_iommu_bypass();
  enable_pcie_peer_dma();
  enable_thunderbolt_dma();
  enable_pcie_bar_mmio(2);
  enable_acpi_dsdt_override();
  enable_usb_dfu_dma();
  apply_all_residuals();
  pull_entities();
  last_.hardware_enabled = enabled_;
  last_.iommu_off = !world_.trust.iommu_on;
  last_.process_list_clean = process_list_is_clean();
  std::ostringstream oss;
  oss << "dma_full_stealth entities=" << last_.entity_count
      << " scatter=" << last_.fpga_scatter_count
      << " peer_txns=" << last_.pcie_peer_txns
      << " clean=" << (last_.process_list_clean ? 1 : 0);
  last_.detail = oss.str();
  world_.note("t4 DmaRadar " + last_.detail);
  return last_;
}

// DmaRadar::apply: RED multi-step plant of educational scars on sim::World.
RedResult DmaRadar::apply() {
  auto rep = run_full_loop(true);
  RedResult r;
  r.process_list_clean = rep.process_list_clean;
  r.offbox_read_ok = rep.offbox_read_ok;
  r.entities = static_cast<std::size_t>(rep.entity_count);
  r.achieved = rep.hardware_enabled && rep.process_list_clean &&
               (rep.offbox_read_ok || rep.entity_count > 0);
  r.detail = rep.detail;
  return r;
}

// RED entry: plant multi-step educational scars on sim::World.
RedResult apply(sim::World& world) {
  DmaRadar radar(world);
  return radar.apply();
}

}  // namespace t4_red
