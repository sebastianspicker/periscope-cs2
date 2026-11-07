// Split from live_radar_stack.cpp, see MONOLITH_REFACTOR_LEDGER.
#include "real/cs2/live_radar_stack.hpp"
#include "real/cs2/live_radar_stack_internal.hpp"
#include "real/cs2/entity_layout.hpp"
#include "real/cs2/handle_sanity.hpp"
#include "real/cs2/offsets_snapshot.hpp"
#include "real/cs2/player_name.hpp"
#include "real/cs2/process.hpp"
#include "real/cs2/view_point.hpp"
#include "real/cs2/weapon_table.hpp"
#include "real/win/api_table.hpp"
#include "real/win/timing.hpp"
#include "real/win/xorstr.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if LR_PLATFORM_WINDOWS
#include "real/win/windows_h.hpp"
#endif

namespace real::cs2::stack {

// ── Scattered entity walk ─────────────────────────────────────────
namespace {

std::uint32_t handle_index_mask() {
  return snapshot::constants::entity_handle_index_mask;
}
std::uint32_t chunk_shift() { return snapshot::constants::entity_chunk_shift; }
std::uint64_t chunk_table_offset() {
  return snapshot::constants::entity_chunk_table_offset;
}
std::uint64_t identity_stride(const Cs2Offsets& o) {
  return o.entity_list_entry ? o.entity_list_entry
: snapshot::constants::entity_identity_stride;
}

bool resolve_handle_fn(RemoteReadFn fn, std::uint64_t entity_list, std::uint32_t handle,
                       const Cs2Offsets& offsets, std::uint64_t& out) {
  if (!handle::handle_usable(handle)) return false;
  const std::uint32_t index = handle & handle_index_mask();
  if (index == 0) return false;
  const std::uint64_t chunk_address =
      entity_list + chunk_table_offset() +
      static_cast<std::uint64_t>(index >> chunk_shift()) * 8ull;
  std::uint64_t chunk = 0;
  if (!rd_t(fn, chunk_address, chunk) || !handle::pointer_usable(chunk)) return false;
  const std::uint64_t stride = identity_stride(offsets);
  const std::uint64_t slot =
      chunk + static_cast<std::uint64_t>(index & ((1u << chunk_shift()) - 1u)) * stride;
  return rd_t(fn, slot, out) && handle::pointer_usable(out);
}

/// Read and sanitise the controller's name (E50). Failure is non-fatal.
void read_name_fn(RemoteReadFn fn, std::uint64_t controller,
                  const Cs2Offsets& offsets, std::string& out) {
  out.clear();
  if (offsets.controller_name == 0 || !handle::pointer_usable(controller)) return;
  std::uint64_t name_ptr = 0;
  if (!rd_t(fn, controller + offsets.controller_name, name_ptr) ||
      !handle::pointer_usable(name_ptr)) {
    return;
  }
  char raw[128]{};
  if (rd(fn, name_ptr, raw, sizeof(raw) - 1)) out = name::sanitize(raw);
}

/// Resolve the active weapon def index (E51). Best effort; a missing chain
/// leaves the index at zero.
void read_weapon_fn(RemoteReadFn fn, std::uint64_t entity_list, std::uint64_t pawn,
                    const Cs2Offsets& offsets, Cs2PlayerEntity& entity) {
  if (offsets.pawn_weapon_services == 0 || offsets.active_weapon == 0 ||
      offsets.econ_attribute_manager == 0 || offsets.econ_item == 0 ||
      offsets.item_def_index == 0) {
    return;
  }
  std::uint64_t services = 0;
  if (!rd_t(fn, pawn + offsets.pawn_weapon_services, services) ||
      !handle::pointer_usable(services)) {
    return;
  }
  std::uint32_t handle_value = 0;
  if (!rd_t(fn, services + offsets.active_weapon, handle_value) ||
      (handle_value & 0xFFFFFFu) == 0xFFFFFFu) {
    return;
  }
  std::uint64_t weapon = 0;
  if (!resolve_handle_fn(fn, entity_list, handle_value, offsets, weapon)) return;
  std::uint16_t def_index = 0;
  if (!rd_t(fn, weapon + offsets.econ_attribute_manager + offsets.econ_item +
                    offsets.item_def_index,
           def_index) ||
      def_index == 0) {
    return;
  }
  entity.weapon_def_index = def_index;
  entity.weapon = weapon::display_name(def_index);
}

/// Read the observer target for the spectator list (E52).
void read_observer_target_fn(RemoteReadFn fn, std::uint64_t pawn,
                             const Cs2Offsets& offsets, Cs2PlayerEntity& entity) {
  if (offsets.observer_target == 0) return;
  std::uint64_t services = 0;
  if (!rd_t(fn, pawn + snapshot::fields::m_pObserverServices, services) ||
      !handle::pointer_usable(services)) {
    return;
  }
  std::uint32_t target = 0;
  if (rd_t(fn, services + offsets.observer_target, target)) {
    entity.observer_target = target;
  }
}

// ── Bulk-read fast path (assessment ledger E11) ─────────────────────
//
// The scattered path costs ~2 reads per controller (chunk pointer + slot)
// plus ~8 small reads per pawn. The bulk path collapses the controller side
// to ONE read of the first controller page and the pawn side to ONE window
// read per live pawn (+1-2 scene-node reads), with a per-chunk pointer cache
// for pawns living outside the first chunk. Any bulk failure falls back to
// the per-field path for that player, the fast path is an optimization,
// never a correctness dependency.

namespace bulk {

layout::ListGeometry geometry_for(const Cs2Offsets& offsets) {
  layout::ListGeometry g;
  g.handle_index_mask = handle_index_mask();
  g.chunk_shift = chunk_shift();
  g.chunk_table_offset = chunk_table_offset();
  g.stride = identity_stride(offsets);
  return g;
}

/// Field spans inside a pawn head covered by one bulk window. Offsets that
/// do not fit under the window cap are omitted (the caller reads those
/// directly, the window extractor's bounds check keeps this honest).
std::vector<layout::FieldSpan> pawn_window_spans(const Cs2Offsets& offsets) {
  std::vector<layout::FieldSpan> spans;
  const auto add = [&spans](std::uint64_t offset, std::size_t size) {
    if (offset != 0) spans.push_back({offset, size});
  };
  add(offsets.entity_team, 4);
  add(offsets.entity_health, 4);
  add(offsets.entity_lifestate, 1);
  add(offsets.entity_viewangles, 12);
  add(offsets.entity_origin, 12);
  add(offsets.entity_velocity, 12);
  add(offsets.entity_view_offset, 12);
  add(offsets.pawn_weapon_services, 8);
  add(snapshot::fields::m_pGameSceneNode, 8);
  add(snapshot::fields::m_ArmorValue, 4);
  add(snapshot::fields::m_entitySpottedState + snapshot::fields::m_bSpotted, 1);
  add(snapshot::fields::m_pObserverServices, 8);
  return spans;
}

/// Pawn fields served from the bulk window (true) or needing direct reads.
struct PawnWindow {
  layout::Window window;
  layout::FieldSpan team{};
  layout::FieldSpan health{};
  layout::FieldSpan life{};
  layout::FieldSpan scene_node{};
  layout::FieldSpan armor{};
  layout::FieldSpan spotted{};
  layout::FieldSpan observer{};
  layout::FieldSpan velocity{};
  layout::FieldSpan view_offset{};
  layout::FieldSpan weapon_services{};
  bool has_team = false, has_health = false, has_life = false;
  bool has_scene = false, has_armor = false, has_spotted = false, has_observer = false;
  bool has_velocity = false, has_view_offset = false, has_weapon_services = false;
};

PawnWindow pawn_window_for(const Cs2Offsets& offsets) {
  PawnWindow w;
  const auto fits = [&w](std::uint64_t offset, std::size_t size,
                         layout::FieldSpan& out) {
    if (offset == 0 || offset >= layout::kMaxWindowBytes ||
        offset + size > layout::kMaxWindowBytes) {
      return false;
    }
    out = layout::FieldSpan{offset, size};
    return true;
  };
  w.has_team = fits(offsets.entity_team, 4, w.team);
  w.has_health = fits(offsets.entity_health, 4, w.health);
  w.has_life = fits(offsets.entity_lifestate, 1, w.life);
  w.has_scene = fits(snapshot::fields::m_pGameSceneNode, 8, w.scene_node);
  w.has_armor = fits(snapshot::fields::m_ArmorValue, 4, w.armor);
  w.has_spotted =
      fits(snapshot::fields::m_entitySpottedState + snapshot::fields::m_bSpotted, 1, w.spotted);
  w.has_observer = fits(snapshot::fields::m_pObserverServices, 8, w.observer);
  w.has_velocity = fits(offsets.entity_velocity, 12, w.velocity);
  w.has_view_offset = fits(offsets.entity_view_offset, 12, w.view_offset);
  w.has_weapon_services = fits(offsets.pawn_weapon_services, 8, w.weapon_services);
  const std::size_t window_bytes =
      layout::required_window_size(pawn_window_spans(offsets));
  if (window_bytes > 0) w.window = layout::Window(std::vector<std::uint8_t>(window_bytes));
  return w;
}

/// One bulk pawn read + field extraction, mirroring read_player_fn's
/// fallbacks. Returns false when a required field is unavailable.
bool read_player_bulk_fn(RemoteReadFn fn, const Cs2Offsets& offsets,
                         std::uint64_t entity_list, std::uint64_t controller,
                         std::uint32_t controller_handle, PawnWindow& spec,
                         std::uint64_t pawn, std::uint64_t local_pawn,
                         Cs2PlayerEntity& entity) {
  if (!spec.window.valid()) return false;
  // One transaction for the whole pawn head.
  if (!fn(pawn, spec.window.data_mutable(), spec.window.size())) {
    return false;
  }

  entity = {};
  std::uint32_t team_raw = 0;
  std::int32_t health = 0;
  std::uint8_t life = 0;
  std::uint64_t scene = 0;
  bool have_core = true;
  have_core = have_core && spec.has_team &&
              spec.window.extract<std::uint32_t>(spec.team, team_raw);
  have_core = have_core && spec.has_health &&
              spec.window.extract<std::int32_t>(spec.health, health);
  have_core = have_core && spec.has_life &&
              spec.window.extract<std::uint8_t>(spec.life, life);
  have_core = have_core && spec.has_scene &&
              spec.window.extract<std::uint64_t>(spec.scene_node, scene);
  if (!have_core) return false;

  entity.team = team_raw & 0xFFu;
  entity.health = health;
  entity.life_state = life;
  entity.is_alive = health > 0 && life == 0;
  entity.is_local_player = (local_pawn != 0 && pawn == local_pawn);

  // Ledger E7 analog on the memory path: plausibility gates before emit.
  if (!layout::PawnSanity::team(team_raw)) return false;
  if (!entity.is_alive && !layout::PawnSanity::health(health) && !entity.is_local_player) {
    return false;  // torn/dead garbage, not a readable player
  }

  auto read_origin = [&](ac::Vec3& out) -> bool {
    if (scene) {
      if (rd_t(fn, scene + snapshot::fields::m_vecAbsOrigin, out)) return true;
    }
    if (offsets.entity_origin != 0 && offsets.entity_origin + 12 <= spec.window.size()) {
      layout::FieldSpan origin_span{offsets.entity_origin, 12};
      ac::Vec3 staged{};
      if (spec.window.extract<ac::Vec3>(origin_span, staged)) {
        out = staged;
        return true;
      }
    }
    return offsets.entity_origin != 0 && rd_t(fn, pawn + offsets.entity_origin, out);
  };

  ac::Vec3 origin{};
  if (!read_origin(origin)) return false;
  entity.origin = origin;

  if (spec.has_armor) {
    std::int32_t armor = 0;
    if (spec.window.extract<std::int32_t>(spec.armor, armor)) entity.armor = armor;
  }
  if (spec.has_velocity) {
    ac::Vec3 velocity{};
    if (spec.window.extract<ac::Vec3>(spec.velocity, velocity)) entity.velocity = velocity;
  }
  if (spec.has_view_offset) {
    ac::Vec3 offset{};
    if (spec.window.extract<ac::Vec3>(spec.view_offset, offset)) {
      entity.view_offset = offset;
      entity.view_offset_known = true;
    }
  }
  if (scene) {
    std::uint8_t dorm = 0;
    if (rd_t(fn, scene + snapshot::fields::m_bDormant, dorm)) entity.dormant = dorm != 0;
  }
  if (spec.has_spotted) {
    std::uint8_t spotted = 1;
    if (spec.window.extract<std::uint8_t>(spec.spotted, spotted)) {
      entity.spotted = spotted != 0;
      entity.spotted_known = true;
    } else {
      entity.spotted = true;
      entity.spotted_known = false;
    }
  }
  // Spectator: observer services + mode (schema).
  {
    std::uint64_t obs = 0;
    if (spec.has_observer && spec.window.extract<std::uint64_t>(spec.observer, obs) && obs) {
      std::int32_t mode = 0;
      if (rd_t(fn, obs + snapshot::fields::m_iObserverMode, mode)) {
        entity.observer_mode = mode;
        entity.is_spectator = (mode > 0);
        if (entity.is_spectator && !entity.is_alive) entity.life_state = 2;
      }
    }
  }
  // E50/E51/E52: identity, weapon and spectator target. All best effort.
  entity.controller_handle = controller_handle;
  read_name_fn(fn, controller, offsets, entity.name);
  read_weapon_fn(fn, entity_list, pawn, offsets, entity);
  read_observer_target_fn(fn, pawn, offsets, entity);
  if (offsets.entity_viewangles) {
    ac::Vec3 ang{};
    if (offsets.entity_viewangles + 12 <= spec.window.size()) {
      const layout::FieldSpan angles_span{offsets.entity_viewangles, 12};
      if (spec.window.extract<ac::Vec3>(angles_span, ang)) entity.eye_angles = ang;
    } else if (rd_t(fn, pawn + offsets.entity_viewangles, ang)) {
      entity.eye_angles = ang;
    }
  }
  return true;
}

}  // namespace bulk

// ── World-entity scan (E53) ─────────────────────────────────────────
namespace {

WorldKind classify_schema_name(const char* name) {
  if (name == nullptr || *name == '\0') return WorldKind::Other;
  if (std::strncmp(name, "C_Weapon", 8) == 0 ||
      std::strncmp(name, "C_Knife", 7) == 0 ||
      std::strncmp(name, "C_AK47", 6) == 0 ||
      std::strncmp(name, "C_DEagle", 8) == 0 ||
      std::strncmp(name, "C_EconEntity", 12) == 0) {
    return WorldKind::Weapon;
  }
  if (std::strstr(name, "Projectile") != nullptr) return WorldKind::Projectile;
  if (std::strstr(name, "Hostage") != nullptr) return WorldKind::Hostage;
  if (std::strstr(name, "Chicken") != nullptr) return WorldKind::Chicken;
  return WorldKind::Other;
}

/// Follow the identity → class-info → name chain. Best effort.
bool read_schema_name_fn(RemoteReadFn fn, std::uint64_t entity, char* out,
                         std::size_t size) {
  if (out == nullptr || size == 0) return false;
  out[0] = '\0';
  std::uint64_t identity = 0;
  if (!rd_t(fn, entity + 0x10, identity) || !handle::pointer_usable(identity)) {
    return false;
  }
  std::uint64_t class_info = 0;
  if (!rd_t(fn, identity + 0x8, class_info) || !handle::pointer_usable(class_info)) {
    return false;
  }
  std::uint64_t container = 0;
  if (!rd_t(fn, class_info + 0x8, container) || !handle::pointer_usable(container)) {
    return false;
  }
  std::uint64_t name_ptr = 0;
  if (!rd_t(fn, container + 0x8, name_ptr) || !handle::pointer_usable(name_ptr)) {
    return false;
  }
  char raw[96]{};
  if (!rd(fn, name_ptr, raw, sizeof(raw) - 1)) return false;
  std::memcpy(out, raw, size - 1);
  out[size - 1] = '\0';
  return out[0] != '\0';
}

}  // namespace

bool read_player_fn(RemoteReadFn fn, const Cs2Offsets& offsets, std::uint64_t entity_list,
                    std::uint64_t controller, std::uint32_t controller_handle,
                    std::uint64_t local_pawn, Cs2PlayerEntity& entity) {
  if (!controller || !offsets.entity_controller_pawn) return false;
  std::uint32_t pawn_handle = 0;
  if (!rd_t(fn, controller + offsets.entity_controller_pawn, pawn_handle) ||
      !handle::handle_usable(pawn_handle))
    return false;
  std::uint64_t pawn = 0;
  if (!resolve_handle_fn(fn, entity_list, pawn_handle, offsets, pawn)) return false;

  entity = {};
  entity.controller_handle = controller_handle;
  entity.pawn_handle = pawn_handle;

  // Pack health (0x34C) + lifeState (0x354) in one IPC ticket when possible.
  std::uint32_t team = 0;
  std::int32_t health = 0;
  std::uint8_t life = 0;
  ac::Vec3 origin{};
  if (!rd_t(fn, pawn + offsets.entity_team, team)) return false;
  if (offsets.entity_lifestate == offsets.entity_health + 8) {
    std::uint8_t blob[16]{};
    if (!rd(fn, pawn + offsets.entity_health, blob, 12)) return false;
    std::memcpy(&health, blob, 4);
    life = blob[8];
  } else {
    if (!rd_t(fn, pawn + offsets.entity_health, health)) return false;
    if (!rd_t(fn, pawn + offsets.entity_lifestate, life)) return false;
  }
  // Early-out dead non-local before origin/angles (big IPC save).
  entity.team = team & 0xFFu;
  entity.health = health;
  entity.life_state = life;
  entity.is_alive = health > 0 && life == 0;
  entity.is_local_player = (local_pawn != 0 && pawn == local_pawn);
  // Scene node first: m_vecAbsOrigin is the current tick's position, while
  // m_vOldOrigin (offsets.entity_origin) lags a tick. One extra read for the
  // node pointer buys a one-tick accuracy win; fall back on node miss.
  std::uint64_t scene = 0;
  rd_t(fn, pawn + snapshot::fields::m_pGameSceneNode, scene);

  auto read_origin = [&](ac::Vec3& out) -> bool {
    if (scene) {
      if (rd_t(fn, scene + snapshot::fields::m_vecAbsOrigin, out)) return true;
    }
    return rd_t(fn, pawn + offsets.entity_origin, out);
  };

  if (!entity.is_alive && !entity.is_local_player) {
    // Still need origin for dead teammate spectator heuristics, cheap path
    if (!read_origin(origin)) return false;
    entity.origin = origin;
    read_name_fn(fn, controller, offsets, entity.name);
    read_observer_target_fn(fn, pawn, offsets, entity);
    // Observer services only
    std::uint64_t obs = 0;
    if (rd_t(fn, pawn + snapshot::fields::m_pObserverServices, obs) && obs) {
      std::int32_t mode = 0;
      if (rd_t(fn, obs + snapshot::fields::m_iObserverMode, mode)) {
        entity.observer_mode = mode;
        entity.is_spectator = mode > 0;
        if (entity.is_spectator) entity.life_state = 2;
      }
    }
    return true;
  }
  if (!read_origin(origin)) return false;
  entity.origin = origin;

  if (offsets.entity_velocity) {
    ac::Vec3 velocity{};
    if (rd_t(fn, pawn + offsets.entity_velocity, velocity)) entity.velocity = velocity;
  }
  if (offsets.entity_view_offset) {
    ac::Vec3 offset{};
    if (rd_t(fn, pawn + offsets.entity_view_offset, offset)) {
      entity.view_offset = offset;
      entity.view_offset_known = true;
    }
  }
  read_name_fn(fn, controller, offsets, entity.name);
  read_weapon_fn(fn, entity_list, pawn, offsets, entity);
  read_observer_target_fn(fn, pawn, offsets, entity);

  if (offsets.entity_viewangles) {
    ac::Vec3 ang{};
    if (rd_t(fn, pawn + offsets.entity_viewangles, ang)) entity.eye_angles = ang;
  }
  {
    std::int32_t armor = 0;
    if (rd_t(fn, pawn + snapshot::fields::m_ArmorValue, armor)) entity.armor = armor;
  }
  // Dormant (reuses the scene-node read above).
  if (scene) {
    std::uint8_t dorm = 0;
    if (rd_t(fn, scene + snapshot::fields::m_bDormant, dorm)) entity.dormant = dorm != 0;
  }
  {
    std::uint8_t spotted = 1;
    if (rd_t(fn,
             pawn + snapshot::fields::m_entitySpottedState + snapshot::fields::m_bSpotted,
             spotted)) {
      entity.spotted = spotted != 0;
      entity.spotted_known = true;
    } else {
      // Unknown is not the same as spotted: strict intel hides on unknown.
      entity.spotted = true;
      entity.spotted_known = false;
    }
  }

  // Spectator: m_pObserverServices + m_iObserverMode (schema)
  {
    std::uint64_t obs = 0;
    if (rd_t(fn, pawn + snapshot::fields::m_pObserverServices, obs) && obs) {
      std::int32_t mode = 0;
      if (rd_t(fn, obs + snapshot::fields::m_iObserverMode, mode)) {
        entity.observer_mode = mode;
        entity.is_spectator = (mode > 0);
        if (entity.is_spectator && !entity.is_alive) entity.life_state = 2;
      }
    }
  }
  return true;
}

}  // namespace

EntityReadResult read_entity_list_scattered(RemoteReadFn read,
                                            const Cs2Offsets& offsets,
                                            const EntityWalkOptions& opt) {
  EntityReadResult result;
  const auto start = std::chrono::high_resolution_clock::now();
  if (!read || !offsets.entity_list) {
    result.error_msg = "missing read/offsets";
    return result;
  }

  std::uint64_t entity_list_ptr = 0;
  if (!rd_t(read, offsets.entity_list, entity_list_ptr) || !entity_list_ptr) {
    result.error_msg = "entity list pointer";
    return result;
  }
  const std::uint64_t entity_list = entity_list_ptr;

  std::uint64_t local_pawn = 0;
  if (offsets.local_player)
    rd_t(read, offsets.local_player, local_pawn);

  const int n = std::max(1, std::min(opt.max_controllers, 64));

  // ── Bulk pre-pass (E11) ────────────────────────────────────────────
  // One read resolves every first-chunk controller slot; per-chunk pointer
  // cache serves pawns in other chunks; each pawn costs one window read.
  const layout::ListGeometry geo = bulk::geometry_for(offsets);
  bulk::PawnWindow pawn_window = bulk::pawn_window_for(offsets);
  std::vector<std::uint8_t> controller_page;
  layout::BatchPlan controller_plan{};
  std::uint64_t chunk0 = 0;
  bool bulk = opt.bulk_reads;
  if (bulk) {
    if (!rd_t(read, entity_list + chunk_table_offset(), chunk0) ||
        !handle::pointer_usable(chunk0)) {
      bulk = false;
    }
  }
  if (bulk) {
    controller_plan = layout::plan_controller_batch(
        chunk0, 1, static_cast<std::uint32_t>(n), geo);
    controller_page.resize(controller_plan.size);
    if (!read(controller_plan.address, controller_page.data(), controller_plan.size)) {
      bulk = false;
      controller_page.clear();
    }
  }
  std::unordered_map<std::uint32_t, std::uint64_t> chunk_cache;

  std::vector<int> order;
  order.reserve(static_cast<size_t>(n));
  for (int i = 1; i <= n; ++i) order.push_back(i);

  // Scatter: Fisher-Yates on controller indices BEFORE resolve
  if (opt.scatter_indices && order.size() > 1) {
    std::uint64_t rng = opt.rng ? *opt.rng: 0x9E3779B97F4A7C15ULL;
    for (size_t i = order.size(); i > 1; --i) {
      rng = rng * 0x5851F42D4C957F2DULL + 0x14057B7EF767814FULL;
      size_t j = static_cast<size_t>(rng % i);
      std::swap(order[i - 1], order[j]);
    }
    if (opt.rng) *opt.rng = rng;
  }

  int steps = 0;
  for (int index: order) {
    std::uint64_t controller = 0;
    bool have_controller = false;
    bool slot_known = false;
    if (bulk) {
      const std::size_t slot_offset =
          controller_plan.slot_offset(static_cast<std::uint32_t>(index), geo);
      if (slot_offset != layout::BatchPlan::npos &&
          slot_offset + sizeof(std::uint64_t) <= controller_page.size()) {
        std::memcpy(&controller, controller_page.data() + slot_offset,
                    sizeof(controller));
        // The page already answered for this slot: an empty slot costs zero
        // reads instead of the two-read resolve fallback.
        slot_known = true;
        have_controller = handle::pointer_usable(controller);
      }
    }
    if (slot_known && !have_controller) continue;
    if (!have_controller &&
        !resolve_handle_fn(read, entity_list, static_cast<std::uint32_t>(index), offsets,
                           controller)) {
      continue;
    }
    Cs2PlayerEntity ent{};
    bool have_entity = false;
    if (bulk) {
      // Pawn handle: inside the controller page when the schema offset fits
      // in the identity stride, otherwise one direct read.
      std::uint32_t pawn_handle = 0;
      const std::size_t controller_slot =
          controller_plan.slot_offset(static_cast<std::uint32_t>(index), geo);
      const std::uint64_t pawn_offset = offsets.entity_controller_pawn;
      bool have_handle = false;
      if (pawn_offset != 0 && controller_slot != layout::BatchPlan::npos &&
          pawn_offset + sizeof(std::uint32_t) <= geo.stride &&
          controller_slot + pawn_offset + sizeof(std::uint32_t) <= controller_page.size()) {
        std::memcpy(&pawn_handle,
                    controller_page.data() + controller_slot + pawn_offset,
                    sizeof(pawn_handle));
        have_handle = pawn_handle != 0;
      } else if (pawn_offset != 0) {
        have_handle = rd_t(read, controller + pawn_offset, pawn_handle) && pawn_handle != 0;
      }
      if (have_handle) {
        // Pawn resolution with a per-chunk pointer cache.
        const std::uint32_t pawn_index = pawn_handle & geo.handle_index_mask;
        const std::uint32_t chunk_number = pawn_index >> geo.chunk_shift;
        std::uint64_t pawn_chunk = 0;
        auto it = chunk_cache.find(chunk_number);
        if (it != chunk_cache.end()) {
          pawn_chunk = it->second;
        } else {
          if (rd_t(read,
                   entity_list + geo.chunk_table_offset +
                       static_cast<std::uint64_t>(chunk_number) * 8ull,
                   pawn_chunk) &&
              handle::pointer_usable(pawn_chunk)) {
            chunk_cache.emplace(chunk_number, pawn_chunk);
          }
        }
        if (pawn_chunk != 0) {
          const std::uint64_t pawn = layout::slot_address(pawn_chunk, pawn_index, geo);
          have_entity = read_player_bulk_fn(read, offsets, entity_list, controller,
                                            static_cast<std::uint32_t>(index),
                                            pawn_window, pawn, local_pawn, ent);
          if (have_entity) {
            ent.controller_handle = static_cast<std::uint32_t>(index);
            ent.pawn_handle = pawn_handle;
          }
        }
      }
    }
    if (!have_entity &&
        !read_player_fn(read, offsets, entity_list, controller,
                        static_cast<std::uint32_t>(index), local_pawn, ent))
      continue;
    if (!opt.include_dormant && ent.dormant && !ent.is_local_player) continue;
    if (ent.is_local_player) result.local_player_index = result.entity_count;
    result.entities.push_back(ent);
    ++result.entity_count;
    if (opt.jitter_us && (++steps % 8) == 0) real::win::yield();
  }
  result.read_successful = true;
  result.read_time_us = static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::high_resolution_clock::now() - start)
.count());
  return result;
}

EntityReadResult read_world_entities_fn(RemoteReadFn read,
                                        const Cs2Offsets& offsets,
                                        const EntityWalkOptions& opt) {
  EntityReadResult result;
  if (!read || !offsets.entity_list) {
    result.error_msg = "missing read/offsets";
    return result;
  }
  std::uint64_t list_ptr = 0;
  if (!rd_t(read, offsets.entity_list, list_ptr) ||
      !handle::pointer_usable(list_ptr)) {
    result.error_msg = "entity list pointer";
    return result;
  }
  const std::uint64_t entity_list = list_ptr;

  const int max = std::max(opt.world_start, std::min(opt.world_max, 4096));
  const int span = std::max(1, (max - opt.world_start) / 2);
  const int start = opt.world_start + (opt.world_phase ? span: 0);
  const int end = std::min(max, start + span);
  if (start >= end) {
    result.read_successful = true;
    return result;
  }

  for (int index = start; index < end; ++index) {
    std::uint64_t entity = 0;
    if (!resolve_handle_fn(read, entity_list, static_cast<std::uint32_t>(index),
                           offsets, entity)) {
      continue;
    }
    char schema[96]{};
    if (!read_schema_name_fn(read, entity, schema, sizeof(schema))) continue;
    const WorldKind kind = classify_schema_name(schema);
    if (kind == WorldKind::Other || kind == WorldKind::None) continue;

    if (kind == WorldKind::Weapon && opt.world_skip_equipped &&
        offsets.entity_owner != 0) {
      std::uint32_t owner = 0;
      if (rd_t(read, entity + offsets.entity_owner, owner) &&
          (owner & 0x7FFFu) != 0 && (owner & 0xFFFFFFu) != 0xFFFFFFu) {
        continue;  // equipped by a live pawn
      }
    }

    ac::Vec3 origin{};
    std::uint64_t scene = 0;
    if (rd_t(read, entity + snapshot::fields::m_pGameSceneNode, scene) &&
        handle::pointer_usable(scene) &&
        rd_t(read, scene + snapshot::fields::m_vecAbsOrigin, origin)) {
      // current-tick origin
    } else if (offsets.entity_origin == 0 ||
               !rd_t(read, entity + offsets.entity_origin, origin)) {
      continue;
    }

    Cs2PlayerEntity ent{};
    ent.controller_handle = static_cast<std::uint32_t>(index);
    ent.origin = origin;
    ent.world_kind = kind;
    ent.is_alive = true;
    ent.is_hostage = (kind == WorldKind::Hostage);
    if (kind == WorldKind::Weapon && offsets.econ_attribute_manager != 0 &&
        offsets.econ_item != 0 && offsets.item_def_index != 0) {
      std::uint16_t def_index = 0;
      if (rd_t(read, entity + offsets.econ_attribute_manager + offsets.econ_item +
                        offsets.item_def_index,
               def_index) &&
          def_index != 0) {
        ent.weapon_def_index = def_index;
        ent.weapon = weapon::display_name(def_index);
      }
    }
    result.entities.push_back(std::move(ent));
    ++result.entity_count;
    if (result.entities.size() >= 256) break;
  }
  result.read_successful = true;
  return result;
}

Result<Cs2PlayerEntity> read_local_player_fn(RemoteReadFn read,
                                             const Cs2Offsets& offsets) {
  if (!read || !offsets.local_player)
    return {{}, "missing"};
  std::uint64_t local = 0;
  if (!rd_t(read, offsets.local_player, local) || !local)
    return {{}, "local unavailable"};
  Cs2PlayerEntity entity{};
  // *local is the pawn's object pointer, not its 15-bit entity handle.
  entity.pawn_handle = 0;
  entity.is_local_player = true;
  std::uint32_t team = 0;
  std::int32_t health = 0;
  std::uint8_t life = 0;
  ac::Vec3 origin{};
  if (!rd_t(read, local + offsets.entity_team, team)) return {{}, "team"};
  if (!rd_t(read, local + offsets.entity_health, health)) return {{}, "health"};
  if (!rd_t(read, local + offsets.entity_lifestate, life)) return {{}, "life"};
  // Prefer the scene node's m_vecAbsOrigin (current tick); m_vOldOrigin lags.
  std::uint64_t scene = 0;
  if (rd_t(read, local + snapshot::fields::m_pGameSceneNode, scene) && scene &&
      rd_t(read, scene + snapshot::fields::m_vecAbsOrigin, origin)) {
    // current-tick origin
  } else if (!rd_t(read, local + offsets.entity_origin, origin)) {
    return {{}, "origin"};
  }
  entity.team = team & 0xFFu;
  entity.health = health;
  entity.life_state = life;
  entity.origin = origin;
  entity.is_alive = health > 0 && life == 0;
  if (offsets.entity_viewangles) {
    ac::Vec3 ang{};
    if (rd_t(read, local + offsets.entity_viewangles, ang)) entity.eye_angles = ang;
  }
  if (offsets.entity_velocity) {
    ac::Vec3 velocity{};
    if (rd_t(read, local + offsets.entity_velocity, velocity)) entity.velocity = velocity;
  }
  if (offsets.entity_view_offset) {
    ac::Vec3 offset{};
    if (rd_t(read, local + offsets.entity_view_offset, offset)) {
      entity.view_offset = offset;
      entity.view_offset_known = true;
    }
  }
  return Result<Cs2PlayerEntity>(entity);
}

EntityReadResult read_entity_list_throttled(Cs2MemoryReader& reader,
                                            const Cs2Offsets& offsets,
                                            const EntityWalkOptions& opt,
                                            std::uint32_t /*local_pid*/) {
  // Bridge: use reader.read via static TLS fn
  thread_local Cs2MemoryReader* tls = nullptr;
  tls = &reader;
  auto bridge = [](std::uint64_t a, void* b, std::size_t n) -> bool {
    if (!tls) return false;
    auto rr = tls->read(a, n);
    if (rr.status != ac::Status::Ok || rr.bytes.size() != n) return false;
    std::memcpy(b, rr.bytes.data(), n);
    return true;
  };
  return read_entity_list_scattered(bridge, offsets, opt);
}

}  // namespace real::cs2::stack
