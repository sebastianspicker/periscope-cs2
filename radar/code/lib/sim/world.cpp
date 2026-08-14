// world.cpp — sim::World implementation: processes, handles, drivers, net scars.
// make_arena() seeds the educational OS/game snapshot used by all pairs.

#include "sim/world.hpp"

#include <algorithm>
#include <cstring>

namespace sim {


// World::note: World::note: educational sim residual path.
void World::note(std::string line) {
  log.push_back(line);
  notes.push_back(std::move(line));
}

// World::spawn: Create a Process in the arena; returns new pid.
std::uint32_t World::spawn(std::string name, bool game, bool ac,
                           std::uint32_t parent_pid) {
  const auto pid = next_pid++;
  Process p;
  p.pid = pid;
  p.name = std::move(name);
  p.is_game = game;
  p.is_ac = ac;
  p.parent_pid = parent_pid;
  // Signer is verified identity (not spoofable by renaming); seed known ones.
  if (game) {
    p.signer = "Valve Corp.";
    p.memory.assign(4096, 0);
    p.modules.push_back(Module{"game.exe", p.base, 4096, true, false, "clean"});
    p.modules.push_back(
        Module{"client.dll", p.base + 0x200000, 0x1000, true, false, "clean"});
    p.cr3 = 0x1A2B0000ull + pid;  // lab CR3 token unique per game pid
  }
  if (ac) {
    p.signer = "AC Vendor";
    p.modules.push_back(
        Module{"ac-agent.exe", p.base, 0x2000, true, false, "clean"});
  }
  if (p.signer.empty()) {
    static const std::unordered_map<std::string, std::string> kKnownSigners = {
        {"explorer.exe", "Microsoft Windows"},
        {"svchost.exe", "Microsoft Windows"},
        {"nvcontainer.exe", "NVIDIA Corporation"},
        {"nvidia-overlay", "NVIDIA Corporation"},
        {"RTSS.exe", "Unwinder"},
    };
    const auto it = kKnownSigners.find(p.name);
    if (it != kKnownSigners.end()) {
      p.signer = it->second;
    }
  }
  processes.emplace(pid, std::move(p));
  note("spawn pid=" + std::to_string(pid) + " name=" + processes[pid].name +
       " parent=" + std::to_string(parent_pid));
  return pid;
}

// World::proc: Lookup Process by pid (const/mutable overloads).
Process* World::proc(std::uint32_t pid) {
  auto it = processes.find(pid);
  return it == processes.end() ? nullptr : &it->second;
}

// World::proc: Lookup Process by pid (const/mutable overloads).
const Process* World::proc(std::uint32_t pid) const {
  auto it = processes.find(pid);
  return it == processes.end() ? nullptr : &it->second;
}

// World::game_pid: Pid of the seeded game process in this arena.
std::uint32_t World::game_pid() const {
  for (const auto& [id, p] : processes) {
    if (p.is_game) {
      return id;
    }
  }
  return 0;
}

// World::ac_pid: Pid of the seeded anti-cheat process in this arena.
std::uint32_t World::ac_pid() const {
  for (const auto& [id, p] : processes) {
    if (p.is_ac) {
      return id;
    }
  }
  return 0;
}

bool World::open_process(std::uint32_t owner, std::uint32_t target,
                         AccessMask access, bool syscall_path) {
  if (!proc(owner) || !proc(target)) {
    return false;
  }
  handles.push_back(Handle{owner, target, access, syscall_path, false});
  const auto acq = syscall_path ? ac::HandleAcquisitionModel::DirectSyscall
                                : ac::HandleAcquisitionModel::DirectOpenProcess;
  record_handle_table(owner, target, static_cast<std::uint32_t>(access), acq,
                      /*ephemeral=*/false, /*via_proxy=*/false);
  note(std::string("open_process owner=") + std::to_string(owner) +
       " target=" + std::to_string(target) +
       (syscall_path ? " path=syscall" : " path=winapi"));
  return true;
}

// World::close_handles_from: Close all handles owned by a pid (post-mitigate cleanup).
void World::close_handles_from(std::uint32_t owner) {
  handles.erase(std::remove_if(handles.begin(), handles.end(),
                               [&](const Handle& h) {
                                 return h.owner_pid == owner;
                               }),
                handles.end());
  handle_table.close_all_from(owner, static_cast<std::uint64_t>(lab_match_tick));
}

// World::record_handle_table: Append a HandleTable entry for blue sensors.
void World::record_handle_table(std::uint32_t owner, std::uint32_t target,
                                std::uint32_t access_mask,
                                ac::HandleAcquisitionModel acquisition,
                                bool ephemeral, bool via_proxy,
                                bool hidden_during_enum) {
  handle_table.open(owner, target, access_mask, acquisition,
                    static_cast<std::uint64_t>(lab_match_tick), ephemeral,
                    via_proxy, hidden_during_enum);
}

// World::write_mem: Lab mutator — write bytes into process image memory.
bool World::write_mem(std::uint32_t target_pid, std::uint64_t addr,
                      const void* data, std::size_t size) {
  auto* t = proc(target_pid);
  if (!t || (size != 0 && data == nullptr)) {
    return false;
  }
  if (addr < t->base) {
    return false;
  }
  const auto off = static_cast<std::size_t>(addr - t->base);
  if (off + size > t->memory.size()) {
    t->memory.resize(off + size, 0);
  }
  if (size != 0) {
    std::memcpy(t->memory.data() + off, data, size);
  }
  return true;
}

// World::plant_entity_snapshots: Encode entity rows into game lab memory.
void World::load_driver(Driver d) {
  note("load_driver " + d.name + " order=" + std::to_string(d.load_order));
  drivers.push_back(std::move(d));
}

// World::create_device: Create Device node owned by a driver (ioctl surface).
void World::create_device(Device d) {
  note("create_device " + d.name);
  devices.push_back(std::move(d));
}

// World::device_ioctl_read: Sim ioctl read through a Device with mem_rw_ioctl.
bool World::device_ioctl_read(std::uint32_t opener_pid, const std::string& device,
                              std::uint32_t target_pid, std::uint64_t addr,
                              std::size_t size, std::vector<std::uint8_t>& out) {
  const Device* dev = nullptr;
  for (const auto& d : devices) {
    if (d.name == device && d.mem_rw_ioctl) {
      dev = &d;
      break;
    }
  }
  if (!dev) {
    return false;
  }
  // Blue BYOVD policy: known-bad drivers lose IOCTL after blocklist mitigate.
  if (byovd_policy_block) {
    for (const auto& drv : drivers) {
      if (drv.name == dev->owner_driver && drv.byovd_known_bad) {
        note("device_ioctl_read DENIED (byovd_policy_block) " + device);
        return false;
      }
    }
  }
  auto rr = read_mem(opener_pid, target_pid, addr, size, false);
  if (rr.status != ac::Status::Ok) {
    return false;
  }
  out = std::move(rr.bytes);
  return true;
}

// World::try_start_personal_hv: Attempt personal HV under VBS/HVCI policy; may fail closed.
bool World::try_start_personal_hv(std::string vendor) {
  if (trust.vbs || trust.hvci) {
    note("personal_hv DENIED (VBS/HVCI on)");
    return false;
  }
  trust.personal_hv_active = true;
  trust.hv_vendor = std::move(vendor);
  trust.cpuid_latency_ns = trust.baseline_latency_ns * 8;
  note("personal_hv ACTIVE");
  return true;
}

// World::hv_read: Sim hypervisor-mediated read when personal_hv_active.
bool World::hv_read(std::uint32_t, std::uint32_t target_pid, std::uint64_t addr,
                    std::size_t size, std::vector<std::uint8_t>& out) {
  if (!trust.personal_hv_active) {
    return false;
  }
  auto rr = read_mem(0, target_pid, addr, size, false);
  if (rr.status != ac::Status::Ok) {
    return false;
  }
  out = std::move(rr.bytes);
  return true;
}

// World::dma_read: Sim off-box DMA read when trust.dma_device_present.
bool World::dma_read(std::uint32_t target_pid, std::uint64_t addr,
                     std::size_t size, std::vector<std::uint8_t>& out) {
  // Lab model: FPGA/2nd-PC DMA needs a present bus master and weak remapping.
  if (!trust.dma_device_present) {
    note("dma_read DENIED (no DMA device)");
    return false;
  }
  if (trust.iommu_on &&
      !(iommu_bypass_active && iommu_bypass_confirmed)) {
    note("dma_read DENIED (IOMMU on)");
    return false;
  }
  auto rr = read_mem(0, target_pid, addr, size, /*require_handle=*/false);
  if (rr.status != ac::Status::Ok) {
    return false;
  }
  out = std::move(rr.bytes);
  note("dma_read ok target=" + std::to_string(target_pid) +
       " size=" + std::to_string(size));
  return true;
}

// World::inject_module: Inject a Module residual into a process module list.
bool World::inject_module(std::uint32_t into_pid, Module m, bool manual_map) {
  auto* p = proc(into_pid);
  if (!p) {
    return false;
  }
  if (manual_map) {
    m.linked_in_peb = false;
    m.headers_erased = true;
    p->manual_mapped_region = true;
    p->has_foreign_thread = true;
  }
  p->modules.push_back(std::move(m));
  note("inject into " + std::to_string(into_pid) +
       (manual_map ? " manual_map" : " loadlibrary"));
  return true;
}

// World::add_overlay: Record an OverlayWindow residual owned by a pid.
void World::add_overlay(OverlayWindow o) {
  overlays.push_back(std::move(o));
}

// World::push_input: World::push_input: educational sim residual path.
void World::push_input(InputEvent e) { inputs.push_back(std::move(e)); }

// World::spoof_hwid: Rotate trust.hwid (account-graph / spoof lesson).
void World::spoof_hwid(std::string new_hwid) {
  trust.hwid = std::move(new_hwid);
  note("hwid spoofed");
}

// World::add_account: World::add_account: educational sim residual path.
void World::add_account(Account a) { accounts.push_back(std::move(a)); }

// World::add_net: World::add_net: educational sim residual path.
void World::add_net(NetFlow f) { net.push_back(std::move(f)); }

// World::handles_to: Enumerate handles targeting a pid (optional include closed).
std::vector<Handle> World::handles_to(std::uint32_t target,
                                      bool include_hidden) const {
  std::vector<Handle> out;
  for (const auto& h : handles) {
    if (h.target_pid != target) {
      continue;
    }
    if (!include_hidden && h.hidden_during_enum) {
      continue;
    }
    out.push_back(h);
  }
  return out;
}

// World::count_hidden_handles_to: Count handles to target that claim hidden_from_enum.
int World::count_hidden_handles_to(std::uint32_t target) const {
  int n = 0;
  for (const auto& h : handles) {
    if (h.target_pid == target && h.hidden_during_enum) {
      ++n;
    }
  }
  return n;
}

// World::handles_for: Enumerate handles targeting a pid (alias of handles_to).
std::vector<Handle> World::handles_for(std::uint32_t pid) const {
  return handles_to(pid);
}

// World::clear_inherited_handles: Drop inherited handles owned by or targeting pid.
void World::clear_inherited_handles(std::uint32_t pid) {
  inherited_handles.erase(std::remove_if(inherited_handles.begin(),
                                         inherited_handles.end(),
                                         [&](const Handle& h) {
                                           return h.owner_pid == pid ||
                                                  h.target_pid == pid;
                                         }),
                          inherited_handles.end());
}

// World::add_section: Add a named memory section residual on a process.
void World::add_section(SharedSection s) {
  note("section " + s.name + " creator=" + std::to_string(s.creator_pid));
  sections.push_back(std::move(s));
}

// World::list_processes: Return process table snapshot for blue enum sensors.
std::vector<Process> World::list_processes(bool weak_enum) const {
  std::vector<Process> out;
  for (const auto& [_, p] : processes) {
    if (weak_enum && p.hidden_from_weak_enum) {
      continue;
    }
    out.push_back(p);
  }
  return out;
}

// World::add_service: Register a lab service entry (SCM-shaped residual).
void World::add_service(ServiceEvent s) {
  note("service " + s.name + " image=" + s.driver_image);
  services.push_back(std::move(s));
}

// World::enable_callback_shadow: Mark callback chain as shadowed (T2 residual).
void World::enable_callback_shadow(bool on) {
  if (on && !callback_shadow_active) {
    process_notify_true = process_notify;
    image_notify_true = image_notify;
    ac_callback_true = ac_callback_present;
    // Present a clean façade during single-shot probes.
    process_notify = 4;
    image_notify = 3;
    ac_callback_present = true;
    callback_shadow_active = true;
    note("callback_shadow ON (façade clean)");
  } else if (!on && callback_shadow_active) {
    process_notify = process_notify_true;
    image_notify = image_notify_true;
    ac_callback_present = ac_callback_true;
    callback_shadow_active = false;
    note("callback_shadow OFF (true degraded state visible)");
  }
}

// World::sample_callbacks: Snapshot callback counts for integrity audit.
void World::sample_callbacks(std::size_t& out_pn, std::size_t& out_in,
                             bool& out_ac) const {
  // Single sample sees façade if shadowing; multi-sample blue also checks
  // process_notify_true when shadow is known.
  out_pn = process_notify;
  out_in = image_notify;
  out_ac = ac_callback_present;
}

// make_arena: free function for this educational unit.
World make_arena(const char* game_name) {
  World w;
  w.trust = HostTrust{};
  w.trust.platform_hv_active = false;
  w.driver_allowlist_sha = {"ac_driver_hash"};
  const auto game = w.spawn(game_name, true, false);
  w.spawn("ac-agent", false, true);
  w.plant_lab_entities(game);
  Driver acd{"ac.sys", "ac_driver_hash", "GameCo", true, false, true, false,
             false, w.ac_driver_load_order};
  w.load_driver(std::move(acd));
  w.add_account(Account{"player1", w.trust.hwid, "pay_A", "ip_1", 0});
  return w;
}

// World::advance_match_tick: Multi-match sample clock for delayed confidence lessons.
void World::advance_match_tick(double weak_delta) {
  ++lab_match_tick;
  lab_confidence += weak_delta;
  // Delayed ban candidate only after multiple weak samples (not instant).
  if (lab_match_tick >= 2 && lab_confidence >= 2.0) {
    lab_delayed_ban_ready = true;
    overwatch_queued = true;
  }
  note("advance_match_tick t=" + std::to_string(lab_match_tick) +
       " conf=" + std::to_string(lab_confidence) +
       " ban_ready=" + std::to_string(lab_delayed_ban_ready ? 1 : 0));
}

// World::apply_client_fidelity_budget: Fog+ starve full enemy origins on client path.
void World::apply_client_fidelity_budget(float fidelity, int enemy_budget) {
  if (fidelity < 0.f) fidelity = 0.f;
  if (fidelity > 1.f) fidelity = 1.f;
  client_entity_fidelity = fidelity;
  client_replicated_enemy_budget = enemy_budget;
  if (fidelity < 1.0f || (enemy_budget >= 0 && enemy_budget < 99)) {
    server_sends_full_enemy_origin = false;
    entity_stream_encrypted = true;
    client_has_stream_key = false;
  }
  note("apply_client_fidelity_budget f=" + std::to_string(fidelity) +
       " budget=" + std::to_string(enemy_budget));
}

}  // namespace sim
