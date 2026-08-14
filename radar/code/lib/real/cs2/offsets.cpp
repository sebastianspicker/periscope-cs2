#include "real/cs2/offsets.hpp"
#include "real/cs2/offsets_snapshot.hpp"
#include "real/cs2/process.hpp"

#include "real/memory.hpp"
#include "real/win/xorstr.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

namespace real::cs2 {
namespace {

constexpr std::size_t kScanChunkSize = 0x10000;

std::size_t relative_displacement_offset(const Pattern& pattern) {
  for (std::size_t i = 0; i + sizeof(std::int32_t) <= pattern.mask.size(); ++i) {
    if (pattern.mask.compare(i, sizeof(std::int32_t), "????") == 0) {
      return i;
    }
  }
  return pattern.bytes.size();
}

bool matches_at(const std::vector<std::uint8_t>& buffer, std::size_t offset,
                const Pattern& pattern) {
  for (std::size_t i = 0; i < pattern.bytes.size(); ++i) {
    if (pattern.mask[i] == 'x' && buffer[offset + i] != pattern.bytes[i]) {
      return false;
    }
  }
  return true;
}

void assign_offset(Cs2Offsets& offsets, const Pattern& pattern, std::uint64_t value) {
  if (pattern.name == "dwEntityList") offsets.entity_list = value;
  else if (pattern.name == "dwLocalPlayer" || pattern.name == "dwLocalPlayerPawn") {
    offsets.local_player = value;
  }
  else if (pattern.name == "dwForceAttack") offsets.force_attack = value;
  else if (pattern.name == "dwViewMatrix") offsets.view_matrix = value;
  else if (pattern.name == "dwForceJump") offsets.force_jump = value;
  else if (pattern.name == "dwViewAngles") offsets.view_angles = value;
  else if (pattern.name == "dwSensitivity" || pattern.name == "m_flSensitivity") {
    offsets.sensitivity = value;
  }
  else if (pattern.name == "dwRadarBase") offsets.radar_base = value;
  else if (pattern.name == "entity_controller") {
    offsets.entity_controller = value;
  }
  else if (pattern.name == "entity_controller_pawn") offsets.entity_controller_pawn = value;
  else if (pattern.name == "entity_team" || pattern.name == "m_iTeamNum") offsets.entity_team = value;
  else if (pattern.name == "entity_health" || pattern.name == "m_iHealth") offsets.entity_health = value;
  else if (pattern.name == "entity_lifestate" || pattern.name == "m_lifeState") offsets.entity_lifestate = value;
  else if (pattern.name == "entity_origin" || pattern.name == "m_vOldOrigin") offsets.entity_origin = value;
  else if (pattern.name == "m_angEyeAngles") offsets.entity_viewangles = value;
  else if (pattern.name == "dwRadarPosX" || pattern.name == "radar_pos_x") offsets.radar_pos_x = value;
  else if (pattern.name == "dwRadarPosY" || pattern.name == "radar_pos_y") offsets.radar_pos_y = value;
  else if (pattern.name == "dwRadarScale" || pattern.name == "radar_scale") offsets.radar_scale = value;
  else if (pattern.name == "dwRadarSize" || pattern.name == "radar_size") offsets.radar_size = value;
  else if (pattern.name == "dwGameState" || pattern.name == "game_state") offsets.game_state = value;
}

void apply_schema_fields(Cs2Offsets& o) {
  o.entity_health = snapshot::fields::m_iHealth;
  o.entity_team = snapshot::fields::m_iTeamNum;
  o.entity_lifestate = snapshot::fields::m_lifeState;
  o.entity_origin = snapshot::fields::m_vOldOrigin;
  o.entity_viewangles = snapshot::fields::m_angEyeAngles;
  o.entity_controller_pawn = snapshot::fields::m_hPlayerPawn;
  o.entity_list_entry = snapshot::constants::entity_identity_stride;
}

// Minimal JSON number extractor: finds "key": <number>
bool json_get_u64(const std::string& text, const char* key, std::uint64_t& out) {
  std::string needle = std::string("\"") + key + "\"";
  auto pos = text.find(needle);
  if (pos == std::string::npos) return false;
  pos = text.find(':', pos + needle.size());
  if (pos == std::string::npos) return false;
  ++pos;
  while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\n' ||
                               text[pos] == '\r')) {
    ++pos;
  }
  if (pos >= text.size()) return false;
  char* end = nullptr;
  unsigned long long v = std::strtoull(text.c_str() + pos, &end, 0);
  if (end == text.c_str() + pos) return false;
  out = static_cast<std::uint64_t>(v);
  return true;
}

bool load_snapshot_json_text(const std::string& text, Cs2Offsets& fields_only) {
  // Only schema fields + stride from JSON; globals need client_base at apply time.
  std::uint64_t v = 0;
  if (json_get_u64(text, "m_iHealth", v)) fields_only.entity_health = v;
  if (json_get_u64(text, "m_iTeamNum", v)) fields_only.entity_team = v;
  if (json_get_u64(text, "m_lifeState", v)) fields_only.entity_lifestate = v;
  if (json_get_u64(text, "m_vOldOrigin", v)) fields_only.entity_origin = v;
  if (json_get_u64(text, "m_angEyeAngles", v)) fields_only.entity_viewangles = v;
  if (json_get_u64(text, "m_hPlayerPawn", v)) fields_only.entity_controller_pawn = v;
  if (json_get_u64(text, "entity_identity_stride", v)) fields_only.entity_list_entry = v;
  return fields_only.entity_health != 0 && fields_only.entity_controller_pawn != 0;
}

struct RuntimeGlobals {
  std::uint64_t dwEntityList = 0;
  std::uint64_t dwLocalPlayerPawn = 0;
  std::uint64_t dwViewMatrix = 0;
  std::uint64_t dwViewAngles = 0;
  std::uint64_t dwSensitivity = 0;
  bool valid = false;
};

RuntimeGlobals g_runtime_globals{};

bool load_runtime_globals(const std::string& text) {
  RuntimeGlobals g{};
  bool ok = true;
  ok = json_get_u64(text, "dwEntityList", g.dwEntityList) && ok;
  ok = json_get_u64(text, "dwLocalPlayerPawn", g.dwLocalPlayerPawn) && ok;
  json_get_u64(text, "dwViewMatrix", g.dwViewMatrix);
  json_get_u64(text, "dwViewAngles", g.dwViewAngles);
  json_get_u64(text, "dwSensitivity", g.dwSensitivity);
  if (ok && g.dwEntityList != 0 && g.dwLocalPlayerPawn != 0) {
    g.valid = true;
    g_runtime_globals = g;
    return true;
  }
  return false;
}

std::string read_file_text(const char* path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return {};
  std::ostringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

}  // namespace

std::string ResolvedOffset::describe() const {
  std::ostringstream output;
  output << name << " = 0x" << std::hex << value << std::dec
         << (resolved ? " (resolved)" : " (unresolved)");
  return output.str();
}

Pattern Pattern::from_bytes(const std::string& name,
                            const std::vector<std::uint8_t>& pattern,
                            std::int32_t add_offset, bool is_relative) {
  Pattern result{name, pattern, {}, add_offset, is_relative};
  result.mask.reserve(pattern.size());
  for (const std::uint8_t byte : pattern) {
    result.mask += byte == 0 ? '?' : 'x';
  }
  return result;
}

// Legacy AOB table kept for educational pattern-scan demos.
// Primary live path uses dump RVAs (offsets_from_snapshot).
static auto g_enc_dwEntityList = win::obf::encrypted_pattern<10>(
    {0x48, 0x8B, 0x0D, 0, 0, 0, 0, 0x48, 0x89, 0x7C}, build::kXorKeySeed);
static auto g_enc_dwLocalPlayer = win::obf::encrypted_pattern<8>(
    {0x48, 0x8D, 0x05, 0, 0, 0, 0, 0xC3}, build::kXorKeySeed);
static auto g_enc_dwViewMatrix = win::obf::encrypted_pattern<11>(
    {0x48, 0x8D, 0x0D, 0, 0, 0, 0, 0x48, 0xC1, 0xE0, 0x06}, build::kXorKeySeed);

const std::vector<Pattern>& default_patterns() {
  static const std::vector<Pattern> patterns = []() -> std::vector<Pattern> {
    std::vector<Pattern> result;
    // Schema fields (relative object offsets) — always from snapshot.
    result.emplace_back(Pattern::from_bytes("entity_controller_pawn", {},
        static_cast<std::int32_t>(snapshot::fields::m_hPlayerPawn)));
    result.emplace_back(Pattern::from_bytes("entity_team", {},
        static_cast<std::int32_t>(snapshot::fields::m_iTeamNum)));
    result.emplace_back(Pattern::from_bytes("entity_health", {},
        static_cast<std::int32_t>(snapshot::fields::m_iHealth)));
    result.emplace_back(Pattern::from_bytes("entity_lifestate", {},
        static_cast<std::int32_t>(snapshot::fields::m_lifeState)));
    result.emplace_back(Pattern::from_bytes("entity_origin", {},
        static_cast<std::int32_t>(snapshot::fields::m_vOldOrigin)));
    result.emplace_back(Pattern::from_bytes("m_angEyeAngles", {},
        static_cast<std::int32_t>(snapshot::fields::m_angEyeAngles)));
    result.emplace_back(Pattern::from_bytes("entity_list_entry", {},
        static_cast<std::int32_t>(snapshot::constants::entity_identity_stride)));
    return result;
  }();
  return patterns;
}

Result<std::uint64_t> scan_pattern(std::uint32_t pid, std::uint64_t base,
                                   std::size_t image_size, const Pattern& pattern) {
  if (pattern.bytes.empty() || pattern.bytes.size() != pattern.mask.size()) {
    return {0, "Invalid scan pattern: " + pattern.name};
  }
  if (pattern.bytes.size() > image_size) return {0, "Pattern not found: " + pattern.name};

  const std::size_t overlap = pattern.bytes.size() - 1;
  for (std::size_t offset = 0; offset < image_size;) {
    const std::size_t remaining = image_size - offset;
    const std::size_t requested = std::min(kScanChunkSize + overlap, remaining);
    const auto read = real::read_virtual(pid, base + offset, requested);
    if (read && read.value.size() >= pattern.bytes.size()) {
      for (std::size_t i = 0; i + pattern.bytes.size() <= read.value.size(); ++i) {
        if (!matches_at(read.value, i, pattern)) continue;

        const std::uint64_t found = base + offset + i;
        if (!pattern.relative) return found + pattern.additional_offset;

        const std::size_t displacement = relative_displacement_offset(pattern);
        if (displacement == pattern.bytes.size()) {
          return {0, "Missing rel32 wildcard in pattern: " + pattern.name};
        }
        std::int32_t relative = 0;
        std::memcpy(&relative, read.value.data() + i + displacement, sizeof(relative));
        return static_cast<std::uint64_t>(
            static_cast<std::int64_t>(found + displacement + sizeof(relative)) + relative +
            pattern.additional_offset);
      }
    }
    if (remaining <= kScanChunkSize) break;
    offset += kScanChunkSize;
  }
  return {0, "Pattern not found: " + pattern.name};
}

Result<Cs2Offsets> resolve_offsets(std::uint32_t cs2_pid, std::uint64_t cs2_base,
                                   std::size_t cs2_image_size) {
  // Prefer dump RVAs over AOB when base is client.dll.
  if (cs2_base != 0) {
    return Result<Cs2Offsets>(offsets_from_snapshot(cs2_base));
  }
  Cs2Offsets offsets = offsets_from_snapshot(0);
  for (const Pattern& pattern : default_patterns()) {
    if (pattern.bytes.empty()) {
      assign_offset(offsets, pattern, static_cast<std::uint64_t>(pattern.additional_offset));
      continue;
    }
    const auto result = scan_pattern(cs2_pid, cs2_base, cs2_image_size, pattern);
    assign_offset(offsets, pattern, result ? result.value : 0);
  }
  return Result<Cs2Offsets>(offsets);
}

bool find_module_by_basename(std::uint32_t cs2_pid, std::uint64_t process_handle,
                             const char* basename, std::uint64_t& out_base,
                             std::size_t& out_size) {
  out_base = 0;
  out_size = 0;
  if (!basename || !*basename) return false;
  std::string want = basename;
  for (char& c : want) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  auto modules = get_cs2_modules(cs2_pid, process_handle);
  if (!modules) return false;
  for (const auto& mod : *modules) {
    std::string lower = mod.name;
    for (char& c : lower) {
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    const auto slash = lower.find_last_of("\\/");
    if (slash != std::string::npos) lower = lower.substr(slash + 1);
    if (lower == want && mod.base != 0 && mod.size != 0) {
      out_base = mod.base;
      out_size = mod.size;
      return true;
    }
  }
  return false;
}

bool find_client_module(std::uint32_t cs2_pid, std::uint64_t process_handle,
                        std::uint64_t& out_base, std::size_t& out_size) {
  // Exact basename only — never suffix-match (*client.dll).
  return find_module_by_basename(cs2_pid, process_handle, "client.dll", out_base,
                                 out_size);
}

Cs2Offsets offsets_from_snapshot(std::uint64_t client_base) {
  Cs2Offsets o{};
  apply_schema_fields(o);

  // Prefer runtime JSON globals if loaded; else embedded snapshot.
  const std::uint64_t ent_rva = g_runtime_globals.valid
                                    ? g_runtime_globals.dwEntityList
                                    : snapshot::globals::dwEntityList;
  const std::uint64_t pawn_rva = g_runtime_globals.valid
                                     ? g_runtime_globals.dwLocalPlayerPawn
                                     : snapshot::globals::dwLocalPlayerPawn;
  const std::uint64_t vm_rva = g_runtime_globals.valid
                                   ? g_runtime_globals.dwViewMatrix
                                   : snapshot::globals::dwViewMatrix;
  const std::uint64_t va_rva = g_runtime_globals.valid
                                   ? g_runtime_globals.dwViewAngles
                                   : snapshot::globals::dwViewAngles;
  const std::uint64_t sens_rva = g_runtime_globals.valid
                                     ? g_runtime_globals.dwSensitivity
                                     : snapshot::globals::dwSensitivity;

  if (client_base != 0) {
    // Absolute addresses of globals (read_entity_list dereferences these).
    o.entity_list = client_base + ent_rva;
    o.local_player = client_base + pawn_rva;
    o.view_matrix = client_base + vm_rva;
    o.view_angles = client_base + va_rva;
    o.sensitivity = client_base + sens_rva;
  } else {
    // Store RVAs only (caller must add base later).
    o.entity_list = ent_rva;
    o.local_player = pawn_rva;
    o.view_matrix = vm_rva;
    o.view_angles = va_rva;
    o.sensitivity = sens_rva;
  }
  return o;
}

bool try_load_runtime_snapshot_file(Cs2Offsets& out_fields_only) {
  const char* candidates[] = {
      std::getenv("LR_CS2_OFFSETS_PATH"),
      "offsets_snapshot.json",
      "data/cs2/offsets_snapshot.json",
      "../data/cs2/offsets_snapshot.json",
      "../../data/cs2/offsets_snapshot.json",
  };
  for (const char* path : candidates) {
    if (!path || !*path) continue;
    std::string text = read_file_text(path);
    if (text.empty()) continue;
    Cs2Offsets fields{};
    if (!load_snapshot_json_text(text, fields)) continue;
    load_runtime_globals(text);
    out_fields_only = fields;
    return true;
  }
  return false;
}

Result<Cs2Offsets> resolve_offsets_for_process(std::uint32_t cs2_pid,
                                               std::uint64_t process_handle,
                                               std::uint64_t main_base,
                                               std::size_t main_image_size) {
  (void)main_image_size;

  // Optional runtime override from auto-updater JSON.
  Cs2Offsets runtime_fields{};
  const bool have_runtime = try_load_runtime_snapshot_file(runtime_fields);

  std::uint64_t client_base = 0;
  std::size_t client_size = 0;
  if (!find_client_module(cs2_pid, process_handle, client_base, client_size)) {
    // Fall back to main module (usually cs2.exe) — dump RVAs will not match.
    client_base = main_base;
  }

  Cs2Offsets offsets = offsets_from_snapshot(client_base);
  if (have_runtime) {
    // Overlay schema fields from JSON (globals already applied if present).
    if (runtime_fields.entity_health) offsets.entity_health = runtime_fields.entity_health;
    if (runtime_fields.entity_team) offsets.entity_team = runtime_fields.entity_team;
    if (runtime_fields.entity_lifestate) offsets.entity_lifestate = runtime_fields.entity_lifestate;
    if (runtime_fields.entity_origin) offsets.entity_origin = runtime_fields.entity_origin;
    if (runtime_fields.entity_viewangles) {
      offsets.entity_viewangles = runtime_fields.entity_viewangles;
    }
    if (runtime_fields.entity_controller_pawn) {
      offsets.entity_controller_pawn = runtime_fields.entity_controller_pawn;
    }
    if (runtime_fields.entity_list_entry) {
      offsets.entity_list_entry = runtime_fields.entity_list_entry;
    }
    // Re-apply globals with runtime RVAs if loaded.
    if (g_runtime_globals.valid && client_base != 0) {
      offsets.entity_list = client_base + g_runtime_globals.dwEntityList;
      offsets.local_player = client_base + g_runtime_globals.dwLocalPlayerPawn;
      if (g_runtime_globals.dwViewMatrix)
        offsets.view_matrix = client_base + g_runtime_globals.dwViewMatrix;
      if (g_runtime_globals.dwViewAngles)
        offsets.view_angles = client_base + g_runtime_globals.dwViewAngles;
    }
  }

  return Result<Cs2Offsets>(offsets);
}

Cs2Offsets sim_offsets() {
  Cs2Offsets offsets = offsets_from_snapshot(0);
  // Sim uses synthetic bases; keep relative-looking test values for arena.
  offsets.entity_list = 0x1000;
  offsets.local_player = 0x8000;
  offsets.entity_list_entry = snapshot::constants::entity_identity_stride;
  return offsets;
}

bool Cs2Offsets::is_complete() const {
  return entity_list != 0 && local_player != 0 && entity_controller_pawn != 0 &&
         entity_health != 0 && entity_origin != 0;
}

std::string Cs2Offsets::describe() const {
  std::ostringstream output;
  output << std::hex << std::showbase;
  output << "entity_list=" << entity_list << '\n';
  output << "entity_list_entry=" << entity_list_entry << '\n';
  output << "entity_controller=" << entity_controller << '\n';
  output << "entity_controller_pawn=" << entity_controller_pawn << '\n';
  output << "entity_team=" << entity_team << '\n';
  output << "entity_health=" << entity_health << '\n';
  output << "entity_lifestate=" << entity_lifestate << '\n';
  output << "entity_origin=" << entity_origin << '\n';
  output << "entity_viewangles=" << entity_viewangles << '\n';
  output << "radar_base=" << radar_base << '\n';
  output << "radar_pos_x=" << radar_pos_x << '\n';
  output << "radar_pos_y=" << radar_pos_y << '\n';
  output << "radar_scale=" << radar_scale << '\n';
  output << "radar_size=" << radar_size << '\n';
  output << "local_player=" << local_player << '\n';
  output << "game_state=" << game_state << '\n';
  output << "force_attack=" << force_attack << '\n';
  output << "force_jump=" << force_jump << '\n';
  output << "sensitivity=" << sensitivity << '\n';
  output << "view_matrix=" << view_matrix << '\n';
  output << "view_angles=" << view_angles;
  return output.str();
}

}  // namespace real::cs2
