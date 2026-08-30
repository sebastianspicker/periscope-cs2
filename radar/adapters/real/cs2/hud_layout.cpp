#include "real/cs2/hud_layout.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace real::cs2 {
namespace {

bool read_bytes(Cs2MemoryReader& reader, std::uint64_t addr, void* out,
                std::size_t n) {
  auto rr = reader.read(addr, n);
  if (rr.status != ac::Status::Ok || rr.bytes.size() != n) return false;
  std::memcpy(out, rr.bytes.data(), n);
  return true;
}

bool looks_like(float v, float lo, float hi) {
  return std::isfinite(v) && v >= lo && v <= hi;
}

struct Section {
  std::uint64_t start = 0;
  std::size_t size = 0;
};

// Collect PE sections that typically hold strings / ConVar data (skip .text).
std::vector<Section> pe_data_sections(Cs2MemoryReader& reader,
                                      std::uint64_t base,
                                      std::size_t module_size) {
  std::vector<Section> out;
  std::uint8_t dos[0x40]{};
  if (!read_bytes(reader, base, dos, sizeof(dos))) return out;
  if (dos[0] != 'M' || dos[1] != 'Z') return out;
  std::int32_t e_lfanew = 0;
  std::memcpy(&e_lfanew, dos + 0x3C, 4);
  if (e_lfanew <= 0 || static_cast<std::size_t>(e_lfanew) + 0x108 > module_size)
    return out;

  std::uint8_t nt[0x200]{};
  if (!read_bytes(reader, base + static_cast<std::uint64_t>(e_lfanew), nt,
                  sizeof(nt)))
    return out;
  if (nt[0] != 'P' || nt[1] != 'E') return out;

  std::uint16_t num_sections = 0;
  std::uint16_t opt_size = 0;
  std::memcpy(&num_sections, nt + 6, 2);
  std::memcpy(&opt_size, nt + 20, 2);
  if (num_sections == 0 || num_sections > 32) return out;

  const std::uint64_t sec_table =
      base + static_cast<std::uint64_t>(e_lfanew) + 24 + opt_size;
  for (std::uint16_t i = 0; i < num_sections; ++i) {
    std::uint8_t sh[40]{};
    if (!read_bytes(reader, sec_table + static_cast<std::uint64_t>(i) * 40, sh,
                    40))
      continue;
    char name[9]{};
    std::memcpy(name, sh, 8);
    std::uint32_t vsize = 0, vrva = 0, chars = 0;
    std::memcpy(&vsize, sh + 8, 4);
    std::memcpy(&vrva, sh + 12, 4);
    std::memcpy(&chars, sh + 36, 4);
    // IMAGE_SCN_MEM_EXECUTE = 0x20000000 — skip code.
    if (chars & 0x20000000u) continue;
    if (vsize < 0x100 || vrva == 0) continue;
    if (static_cast<std::size_t>(vrva) >= module_size) continue;
    const std::size_t sz =
        std::min<std::size_t>(vsize, module_size - static_cast<std::size_t>(vrva));
    // Prefer rdata/data/reloc-free named sections; still accept any non-exec.
    out.push_back({base + vrva, sz});
  }
  return out;
}

struct CvarTarget {
  const char* name;
  float lo;
  float hi;
  float* out;
  float best = 0.f;
  float best_score = -1.f;
  bool found = false;
};

void resolve_cvars_batch(Cs2MemoryReader& reader, std::uint64_t base,
                         std::size_t size, CvarTarget* targets, int n_targets) {
  if (!base || size < 0x1000 || !targets || n_targets <= 0) return;

  auto sections = pe_data_sections(reader, base, size);
  if (sections.empty()) {
    // Fallback: scan whole module (slow path).
    sections.push_back({base, size});
  }

  std::unordered_map<std::uint64_t, int> string_to_target;
  constexpr std::size_t kChunk = 0x10000;
  std::vector<std::uint8_t> buf(kChunk + 128);

  // Pass 1: name strings in data sections
  for (const Section& sec : sections) {
    for (std::size_t off = 0; off < sec.size;) {
      const std::size_t n = std::min(kChunk + 64, sec.size - off);
      if (!read_bytes(reader, sec.start + off, buf.data(), n)) {
        off += kChunk;
        continue;
      }
      for (int t = 0; t < n_targets; ++t) {
        const char* name = targets[t].name;
        const std::size_t len = std::strlen(name);
        for (std::size_t i = 0; i + len < n; ++i) {
          if (std::memcmp(buf.data() + i, name, len) != 0) continue;
          if (i + len < n && buf[i + len] != 0) continue;
          string_to_target[sec.start + off + i] = t;
        }
      }
      off += kChunk;
    }
  }
  if (string_to_target.empty()) return;

  // Pass 2: pointer refs + float probes in data sections only
  constexpr int kValueOffs[] = {0x28, 0x2C, 0x30, 0x34, 0x40, 0x44, 0x48, 0x50,
                                0x54, 0x58, 0x60, 0x18, 0x20, 0x24};

  for (const Section& sec : sections) {
    for (std::size_t off = 0; off + 8 <= sec.size;) {
      const std::size_t n = std::min(kChunk, sec.size - off);
      if (!read_bytes(reader, sec.start + off, buf.data(), n)) {
        off += kChunk;
        continue;
      }
      for (std::size_t i = 0; i + 8 <= n; i += 8) {
        std::uint64_t ptr = 0;
        std::memcpy(&ptr, buf.data() + i, 8);
        auto it = string_to_target.find(ptr);
        if (it == string_to_target.end()) continue;

        CvarTarget& tgt = targets[it->second];
        const std::uint64_t ptr_addr = sec.start + off + i;
        for (int slot : {0, 8, 16, 24}) {
          const std::uint64_t obj = ptr_addr - static_cast<std::uint64_t>(slot);
          for (int vo : kValueOffs) {
            float v = 0.f;
            if (!read_bytes(reader, obj + static_cast<std::uint64_t>(vo), &v,
                            sizeof(v)))
              continue;
            if (!looks_like(v, tgt.lo, tgt.hi)) continue;
            const float mid = 0.5f * (tgt.lo + tgt.hi);
            const float score = 1.f / (std::fabs(v - mid) + 0.05f);
            if (score > tgt.best_score) {
              tgt.best_score = score;
              tgt.best = v;
              tgt.found = true;
            }
          }
        }
      }
      off += kChunk;
    }
  }

  for (int t = 0; t < n_targets; ++t) {
    if (targets[t].found && targets[t].out) *targets[t].out = targets[t].best;
  }
}

}  // namespace

RadarHudSettings read_radar_hud_settings(Cs2MemoryReader& reader,
                                         std::uint64_t client_base,
                                         std::size_t client_size) {
  RadarHudSettings s{};
  if (!client_base || client_size == 0 || !reader.is_attached()) return s;

  // Optional fast path: skip expensive scan when env set.
  if (const char* skip = std::getenv("LR_SKIP_CVAR_SCAN");
      skip && skip[0] == '1') {
    return s;
  }

  CvarTarget targets[] = {
      {"hud_scaling", 0.50f, 1.15f, &s.hud_scaling},
      {"cl_hud_radar_scale", 0.75f, 1.35f, &s.cl_hud_radar_scale},
      {"cl_radar_scale", 0.20f, 1.05f, &s.cl_radar_scale},
      {"safezonex", 0.50f, 1.00f, &s.safezonex},
      {"safezoney", 0.50f, 1.00f, &s.safezoney},
  };
  resolve_cvars_batch(reader, client_base, client_size, targets,
                      static_cast<int>(sizeof(targets) / sizeof(targets[0])));

  // Also try engine2.dll if present (some cvars live there).
  // Caller can re-invoke with engine base later; keep client-only for now.

  int n = 0;
  for (auto& t : targets)
    if (t.found) ++n;
  s.resolved_count = n;
  s.valid = n > 0;
  return s;
}

void apply_hud_settings_to_style(const RadarHudSettings& hud,
                                 real::gpu::OverlayStyle& style) {
  if (!hud.valid) return;
  style.hud_scaling = hud.hud_scaling;
  style.cl_hud_radar_scale = hud.cl_hud_radar_scale;
  style.cl_radar_scale = hud.cl_radar_scale;
  style.safezonex = hud.safezonex;
  style.safezoney = hud.safezoney;
}

real::gpu::InGameRadarLayout compute_radar_screen_rect(
    void* game_hwnd, const RadarHudSettings& hud,
    const real::gpu::OverlayStyle& style) {
  real::gpu::OverlayStyle s = style;
  apply_hud_settings_to_style(hud, s);
  return real::gpu::compute_ingame_radar_layout(game_hwnd, s);
}

}  // namespace real::cs2
