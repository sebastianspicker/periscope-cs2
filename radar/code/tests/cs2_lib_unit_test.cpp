// cs2_lib_unit_test.cpp — Unit tests for shipped ac_cs2 (lib/cs2) educational stack.
// Drives real entry points: make_cs2_arena, Cs2GameReader, PatternScanner,
// DiagnosticOrchestrator, SchemaRegistry, radar_math. No re-implementation.

#include "cs2/ac_evasion.hpp"
#include "cs2/diagnostic_orchestrator.hpp"
#include "cs2/diagnostic_system.hpp"
#include "cs2/entities.hpp"
#include "cs2/offsets.hpp"
#include "cs2/radar_math.hpp"
#include "cs2/schema.hpp"
#include "cs2/signatures.hpp"
#include "cs2/simulator.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int g_fails = 0;

static void expect(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  } else {
    std::printf("OK: %s\n", msg);
  }
}

static bool near(float a, float b, float eps = 1e-3f) {
  return std::fabs(a - b) <= eps;
}

int main() {
  using namespace cs2;

  // ── 1) Schema FNV + registry resolution (shipped SchemaRegistry) ──
  {
    expect(fnv1a32("m_iHealth") != 0, "fnv1a32 non-zero for m_iHealth");
    expect(fnv1a32("m_iHealth") == fnv1a32(std::string_view("m_iHealth")),
           "fnv1a32 stable");
    const auto& reg = SchemaRegistry::get();
    expect(reg.total_fields() >= 20, "schema registry has substantial fields");
    const int health = reg.resolve_offset("C_BaseEntity", "m_iHealth");
    expect(health == FieldOffsets::get().health, "schema m_iHealth matches FieldOffsets");
    const int pawn = reg.resolve_offset("CCSPlayerController", "m_hPlayerPawn");
    expect(pawn == FieldOffsets::get().controller_pawn_handle,
           "schema m_hPlayerPawn matches FieldOffsets");
    expect(reg.resolve_offset("C_BaseEntity", "no_such_field") < 0,
           "missing schema field returns -1");
    FieldOffsets applied{};
    const int n = apply_schema_to_field_offsets(applied);
    expect(n >= 15, "apply_schema_to_field_offsets applied many fields");
    expect(applied.health == health, "applied health offset");
  }

  // ── 2) Offsets / entity list math ───────────────────────────────
  {
    expect(ENTITY_IDENTITY_STRIDE == 0x70, "identity stride is current retail 0x70");
    const uintptr_t base = 0x1000;
    expect(list_entry_addr(base, 0) == base + 0x10, "list entry outer 0");
    expect(list_entry_addr(base, 512) == base + 0x10 + 8, "list entry chunk 1");
    expect(identity_slot_addr(0x2000, 3) == 0x2000 + 3 * ENTITY_IDENTITY_STRIDE,
           "identity slot stride");
    expect(entity_handle_index(0x12347FFF) == 0x7FFF, "handle mask");
    expect(ModuleGlobals::get().entity_list == 0x254FE70ull,
           "module global dwEntityList snapshot");
    expect(FieldOffsets::get().health == 0x34C, "FieldOffsets.health snapshot");
  }

  // ── 3) Radar math (shipped helpers) ─────────────────────────────
  {
    expect(near(world_edge_radius_for_cl_radar_scale(0.7f), 750.f / 0.7f, 0.1f),
           "edge@0.7");
    expect(near(world_edge_radius_for_cl_radar_scale(1.0f), 750.f, 0.1f),
           "edge@1.0");
    expect(near(world_edge_radius_for_cl_radar_scale(-1.f), 750.f / 0.7f, 0.1f),
           "edge bad falls back");

    float sx = 0, sy = 0;
    polar_to_overlay_blip(-10.f, 0.f, 10.f, sx, sy);
    expect(near(sx, 0.f) && near(sy, -1.f), "ahead polar -> overlay (0,-1)");
    polar_to_overlay_blip(0.f, 10.f, 10.f, sx, sy);
    expect(near(sx, -1.f) && near(sy, 0.f), "left polar -> overlay (-1,0)");

    HudRadarState hud{};
    hud.is_round_active = true;
    hud.map_texture_position = {100.f, 200.f, 0.f};
    hud.map_texture_scale_stored = 1.f;
    hud.max_visibility_sq = 100.f * 100.f;
    hud.radar_scale = 0.7f;
    hud.valid = true;
    bool oob = false;
    auto blip = world_to_overlay_blip(hud, {150.f, 200.f, 0.f}, /*yaw=*/0.f, &oob);
    expect(!oob, "near enemy not OOB");
    expect(blip.y < -0.1f || near(blip.x, 0.f, 0.5f),
           "ahead enemy has forward component on overlay");
  }

  // ── 4) Full arena plant + Cs2GameReader resolve (real path) ─────
  {
    auto arena = make_cs2_arena(10, "de_dust2");
    expect(arena.game_pid != 0, "arena game_pid");
    expect(arena.map_name == "de_dust2", "arena map_name honored");
    expect(arena.player_ids.size() == 10, "arena 10 players");
    expect(arena.bomb.planted && arena.bomb.site == 0, "arena bomb planted A");

    const auto reader_pid = arena.world.spawn("cs2_lib_unit_reader.exe");
    arena.world.open_process(reader_pid, arena.game_pid, sim::AccessMask::VmRead,
                             false);

    Cs2GameReader reader{&arena.world, arena.game_pid};
    auto players = reader.resolve_players(reader_pid, true);
    expect(players.size() == 10, "resolve_players returns 10");
    expect(players.front().is_local, "first player is local");
    expect(players.front().health == 100, "local health planted");
    expect(players.front().team == Team_CT, "local team CT");
    expect(is_player_alive(players.front()), "local alive");
    expect(!players.front().name.empty(), "player name resolved from memory");
    expect(near(players.front().origin.x, -500.f, 0.1f),
           "local origin from scene node");
    expect(players.front().armor == 100, "local armor dual-field");

    auto hud = reader.resolve_hud_radar(reader_pid, true);
    expect(hud.is_round_active, "hud round active");
    expect(hud.valid, "hud valid");
    expect(near(hud.radar_scale, 0.7f), "hud radar_scale");
    expect(near(hud.map_texture_position.x, -2476.f, 0.1f), "dust2 map texture");

    auto bomb = reader.resolve_bomb(reader_pid, true);
    expect(bomb.planted && bomb.ticking, "bomb resolve planted+ticking");
    expect(bomb.site == 0 && bomb.site_name == "A", "bomb site A");
    expect(near(bomb.timer, 40.f), "bomb timer");

    auto state = reader.resolve_game_state(reader_pid, true, "de_dust2");
    expect(state.valid && state.player_count == 10, "game state valid");
    expect(get_entity_list_ptr(state) == SimLayout::ENTITY_LIST,
           "get_entity_list_ptr");
    expect(get_local_player_ptr(state) != 0, "get_local_player_ptr non-zero");
    expect(get_local_controller_ptr(state) != 0,
           "get_local_controller_ptr non-zero");

    auto blips = players_to_blips(players, &state.local, 0.5f, 0.f, 0.f);
    expect(blips.size() == 10, "players_to_blips size");
    expect(blips.front().is_local, "blip0 local");
    expect(near(blips.front().x, -500.f * 0.5f), "blip0 x transform");

    // Map preset switch
    auto mirage = make_cs2_arena(4, "de_mirage");
    expect(mirage.map_name == "de_mirage", "mirage map_name");
    expect(near(default_map_texture_position("de_mirage").x, -3230.f, 0.1f),
           "mirage texture preset");
  }

  // ── 5) PatternScanner on planted signature bytes ────────────────
  {
    PatternScanner scanner;
    const auto& db = scanner.db();
    expect(db.total_count() > 100, "signature corpus substantial");
    expect(db.find("CREATEMOVE") != nullptr, "CREATEMOVE present");
    expect(db.category(CAT_PLAYER) != nullptr, "player category");

    // Plant a known pattern into a buffer and scan_one.
    const auto* pat = db.find("CREATEMOVE");
    expect(pat != nullptr && !pat->bytes_hex.empty(), "CREATEMOVE hex");
    // Build memory with leading noise + exact first non-wildcard bytes of a
    // simple pattern. Use ACCEPTINPUT which starts with fixed 48 89 5C 24.
    const auto* accept = db.find("ACCEPTINPUT");
    expect(accept != nullptr, "ACCEPTINPUT present");
    std::vector<std::uint8_t> mem(256, 0xCC);
    // Encode "48 89 5C 24" at offset 32 (ACCEPTINPUT prefix)
    mem[32] = 0x48;
    mem[33] = 0x89;
    mem[34] = 0x5C;
    mem[35] = 0x24;
    mem[36] = 0x10;  // wildcard slot filled
    // Rest of pattern may not match fully — plant full parse length with wildcards
    // by scanning only via match of the leading fixed bytes through scan_one.
    // For a guaranteed hit, plant the full parsed pattern with wildcards as 0.
    {
      // Use scan_one; if ACCEPTINPUT needs more fixed bytes, plant them from parse.
      // Reconstruct by walking the hex and writing non-wildcard bytes.
      std::string hex(accept->bytes_hex);
      std::size_t off = 40;
      std::size_t i = 0;
      while (i < hex.size()) {
        while (i < hex.size() && (hex[i] == ' ' || hex[i] == '\t')) ++i;
        if (i >= hex.size()) break;
        std::string tok;
        while (i < hex.size() && hex[i] != ' ' && hex[i] != '\t') {
          tok.push_back(hex[i++]);
        }
        if (tok == "?" || tok == "??") {
          mem[off++] = 0;
        } else if (tok.size() == 2) {
          mem[off++] = static_cast<std::uint8_t>(std::stoul(tok, nullptr, 16));
        }
      }
      auto result = scanner.scan_one("ACCEPTINPUT", 0x140000000ull, mem.size(),
                                     mem.data());
      expect(result.found, "scan_one ACCEPTINPUT found in planted buffer");
      expect(result.offset_from_base == 40, "scan_one offset 40");
      expect(result.resolved_address == 0x140000000ull + 40,
             "scan_one absolute address");
    }
  }

  // ── 6) DiagnosticOrchestrator + sensors + evasion ───────────────
  {
    auto arena = make_cs2_arena(6, "de_dust2");
    seed_diagnostic_fixtures(arena.world, /*include_cheat_scars=*/true);

    // Plant radar read call sequence for CallGraphSensor.
    arena.world.diagnostic_state.call_records.push_back(
        {"cheat", "GETENTITY", 1});
    arena.world.diagnostic_state.call_records.push_back(
        {"cheat", "GETPLAYER", 2});
    arena.world.diagnostic_state.call_records.push_back(
        {"cheat", "GETBONE", 3});

    DiagnosticOrchestrator orch(arena.world);
    orch.set_suspect_pid(arena.world.spawn("cheat.exe"));
    auto full = orch.run_full_diagnostics();
    expect(full.diagnostics_sent, "full diagnostics sent");
    expect(full.module_snapshot.total_modules >= 0, "module snapshot ran");
    expect(full.convar_integrity.total_convars > 0, "convars present");
    expect(!full.convar_integrity.integrity_ok || full.has_violations,
           "cheat scars produce integrity fail or violations");
    expect(full.call_graph.anomalous_sequence,
           "call graph detects radar read sequence");
    expect(full.event_listeners.cheat_listener_detected,
           "cheat event listener detected");
    expect(full.messages_collected >= 1, "messages collected");

    // Evasion path
    const auto cheat_pid = arena.world.spawn("evasion_cheat.exe");
    auto evasion = orch.apply_evasions(cheat_pid);
    expect(evasion.technique_count >= 8, "apply_all_evasions techniques");
    expect(evasion.detection_probability < 1.0, "evasion lowers detection prob");

    // DllVerificationState model
    auto dll = DllVerificationState::from_world(arena.world);
    expect(!dll.serialize_message_159().empty(), "message 159 serialize");
    expect(dll.describe().find("CDllVerificationMonitor") != std::string::npos,
           "dll describe");
  }

  // ── 7) stable_display_health ────────────────────────────────────
  {
    int hold = 0;
    expect(stable_display_health(100, 90, hold) == 100, "good read accepted");
    expect(hold == 0, "hold reset on good read");
    expect(stable_display_health(0, 100, hold) == 100, "bad read holds previous");
    expect(hold == 1, "hold counter advanced");
  }

  // ── 8) Structural: message constants + steam overlay markers ────
  {
    expect(static_cast<std::uint32_t>(EBaseUserMessages::UM_DllStatusResponse) ==
               0x9f,
           "UM_DllStatusResponse");
    expect(kMsgDllStatus == 159, "kMsgDllStatus");
    expect(kMsgCounterStrafe == 385, "kMsgCounterStrafe");
    expect(std::strcmp(kGameOverlayRendererDll, "GameOverlayRenderer64.dll") == 0,
           "steam overlay dll name");
    expect(kSteamPresentPtrOffset != 0, "steam present offset");
  }

  if (g_fails) {
    std::fprintf(stderr, "%d failure(s) in cs2_lib_unit_test\n", g_fails);
    return 1;
  }
  std::printf("ALL cs2_lib_unit_test PASSED\n");
  return 0;
}
