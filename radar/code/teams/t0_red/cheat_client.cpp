// cheat_client.cpp — T0 red client: attach, RPM read path, entity pull for radar.
// Orchestrates backend + EntityPipeline on sim::World.
// Enhanced with real backend paths (Hijack, Rpm).

#include "t0_red/cheat_client.hpp"

#include <cstring>
#include <cstdio>
#include <sstream>

namespace t0_red {

// CheatClient::CheatClient: Bind T0 red client to a sim World and process name.
CheatClient::CheatClient(sim::World& world, std::string process_name,
                         MemoryBackend backend_type)
    : world_(world), name_(std::move(process_name)),
      backend_type_(backend_type) {
  pid_ = world_.spawn(name_);
  ui_.set_title(name_);
}

// CheatClient::attach_to_game: Open game process access path for this red client (sim handles).
bool CheatClient::attach_to_game(bool syscall_path) {
  game_pid_ = world_.game_pid();
  if (!game_pid_) {
    last_.detail = "no_game";
    return false;
  }
  // Ensure entity bytes exist (make_arena plants them; re-plant if empty).
  auto* g = world_.proc(game_pid_);
  if (g && g->memory.size() >= 4) {
    std::uint32_t c = 0;
    std::memcpy(&c, g->memory.data(), 4);
    if (c == 0) {
      world_.plant_lab_entities(game_pid_);
    }
  } else if (g) {
    world_.plant_lab_entities(game_pid_);
  }

  const auto st =
      backend_.attach_world(world_, pid_, game_pid_, syscall_path);
  attached_ = (st == ac::Status::Ok);
  last_.attached = attached_;
  last_.foreign_vm_read_handles = count_own_vm_read_();
  last_.detail = attached_ ? "attached_vm_read" : "attach_failed";
  return attached_;
}

// CheatClient::pull_entities: Refresh entity pipeline and cache living set for radar UI.
bool CheatClient::pull_entities() {
  entities_.clear();
  if (!attached_ || !backend_.is_attached()) {
    last_.entities_ok = false;
    last_.detail = "not_attached";
    return false;
  }
  auto* g = world_.proc(game_pid_);
  if (!g) {
    return false;
  }
  EntityPipeline pipe(backend_);
  pipe.set_read_strategy(read_strategy_);
  const int reads_before = backend_.read_ops();
  const auto st = pipe.refresh(g->base);
  if (st != ac::Status::Ok) {
    last_.entities_ok = false;
    last_.detail = "refresh_failed";
    return false;
  }
  entities_ = pipe.entities();
  last_.entities_ok = !entities_.empty();
  last_.entity_count = static_cast<int>(entities_.size());
  last_.bytes_read = backend_.bytes_read_total();
  last_.read_ops = backend_.read_ops();
  if (read_strategy_ != ReadStrategy::Sequential) {
    world_.scattered_read_pattern = true;
    world_.scattered_read_count += backend_.read_ops() - reads_before;
    world_.read_timing_jitter = true;
    last_.scattered_reads = true;
  }
  last_.detail = pipe.stats().detail;
  return last_.entities_ok;
}

// CheatClient::render_radar: Format cached entities into radar UI strings (demo only).
void CheatClient::render_radar(bool register_overlay) {
  blips_.clear();
  ui_.set_local_team(1);  // lab: local on team 1, show team 2 as enemies
  ui_.update(entities_, ac::Vec3{0, 0, 0});
  // Radar canvas is 360x360; map coords are world-relative. Convert to screen
  // pixels so educational tests can assert non-negative projection space.
  constexpr float kCanvas = 360.f;
  constexpr float kHalf = kCanvas * 0.5f;
  constexpr float kScale = 0.5f;  // world units → pixels
  for (const auto& b : ui_.blips()) {
    Blip out;
    out.x = b.map_x;
    out.y = b.map_y;
    out.world_x = b.world_x;
    out.world_y = b.world_z;
    out.screen_x = kHalf + b.map_x * kScale;
    out.screen_y = kHalf - b.map_y * kScale;
    if (out.screen_x < 0.f) out.screen_x = 0.f;
    if (out.screen_y < 0.f) out.screen_y = 0.f;
    if (out.screen_x > kCanvas) out.screen_x = kCanvas;
    if (out.screen_y > kCanvas) out.screen_y = kCanvas;
    out.team = b.team;
    blips_.push_back(out);
  }
  if (register_overlay) {
    ui_.present_external_window(world_, pid_);
  }
  last_.radar_ok = !blips_.empty() || entities_.empty();
  last_.blip_count = static_cast<int>(blips_.size());
}

// CheatClient::run_full_loop: Red attach/read then blue sensors/mitigate on one World tick.
CheatClientReport CheatClient::run_full_loop(bool syscall_path,
                                             bool register_overlay) {
  last_ = {};
  if (!attach_to_game(syscall_path)) {
    return last_;
  }
  if (!pull_entities()) {
    return last_;
  }
  render_radar(register_overlay);
  last_.foreign_vm_read_handles = count_own_vm_read_();
  last_.bytes_read = backend_.bytes_read_total();
  last_.read_ops = backend_.read_ops();
  std::ostringstream oss;
  oss << "full_loop entities=" << last_.entity_count
      << " blips=" << last_.blip_count
      << " handles=" << last_.foreign_vm_read_handles
      << " reads=" << last_.read_ops << " bytes=" << last_.bytes_read;
  last_.detail = oss.str();
  world_.note("t0 CheatClient " + last_.detail);
  return last_;
}
// CheatClient::detach: Clear attach state.
void CheatClient::detach() {
  backend_.detach();
#if LR_PLATFORM_WINDOWS
  hijack_reader_.shutdown();
#endif
  attached_ = false;
}

// ── Real backend methods ─────────────────────────────────────────

bool CheatClient::attach_hijack(std::uint32_t cs2_pid) {
#if LR_PLATFORM_WINDOWS
  auto& api = real::win::g_Api();
  if (!api.resolved) {
    last_.detail = "api_table_not_resolved";
    return false;
  }
  game_pid_ = cs2_pid;
  if (!hijack_reader_.setup(cs2_pid)) {
    last_.detail = "hijack_setup_failed";
    return false;
  }
  attached_ = true;
  last_.attached = true;
  last_.detail = "hijack_attached_donor=" +
                 std::to_string(hijack_reader_.donor_pid());
  world_.note("t0 CheatClient attach_hijack cs2=" + std::to_string(cs2_pid) +
              " donor=" + std::to_string(hijack_reader_.donor_pid()));
  return true;
#else
  (void)cs2_pid;
  last_.detail = "hijack_unavailable";
  return false;
#endif
}

size_t CheatClient::collect_periscope_entities(
    uint64_t entity_list_addr,
    bool (*read_fn)(uint64_t addr, void* buf, size_t size)) {
#if LR_PLATFORM_WINDOWS
  size_t count = periscope_collector_.collect(entity_list_addr, read_fn);
  periscope_local_ = periscope_collector_.local();
  return count;
#else
  (void)entity_list_addr;
  (void)read_fn;
  return 0;
#endif
}

bool CheatClient::read_hud_snapshot(
    uint64_t c_hud_addr, uint64_t client_base, size_t client_size,
    bool (*read_fn)(uint64_t addr, void* buf, size_t size)) {
#if LR_PLATFORM_WINDOWS
  return periscope_hud_reader_.update_snapshot(
      c_hud_addr, client_base, client_size, read_fn);
#else
  (void)c_hud_addr;
  (void)client_base;
  (void)client_size;
  (void)read_fn;
  return false;
#endif
}

// CheatClient::count_own_vm_read_: Count this client's VmRead handles into the game.
int CheatClient::count_own_vm_read_() const {
  int n = 0;
  if (!game_pid_) {
    return 0;
  }
  for (const auto& h : world_.handles_to(game_pid_, true)) {
    if (h.owner_pid == backend_.reader_pid() &&
        sim::has(h.access, sim::AccessMask::VmRead)) {
      ++n;
    }
  }
  return n;
}

}  // namespace t0_red
