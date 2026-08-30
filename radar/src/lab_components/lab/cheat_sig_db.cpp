#include "lab/cheat_sig_db.hpp"

#include <algorithm>
#include <utility>

namespace lab {

CheatSignatureDb::CheatSignatureDb() { seed_known_cheats(); }

CheatSignatureDb& CheatSignatureDb::instance() {
  static CheatSignatureDb database;
  return database;
}

CheatSignature CheatSignatureDb::make_sig(
    std::string_view name, std::string_view version, std::string_view family,
    std::initializer_list<std::uint8_t> bytes, bool known) const {
  CheatSignature sig;
  sig.name = std::string(name);
  sig.version = std::string(version);
  sig.family = std::string(family);
  sig.code_sample.assign(bytes);
  sig.bytes_to_match = sig.code_sample.size();
  sig.known_cheat = known;
  return sig;
}

void CheatSignatureDb::seed_known_cheats() {
  if (seeded_) return;
  seeded_ = true;

  // Educational prologues: x64 push/mov/sub rsp frames + distinctive immediates.
  // These are NOT real commercial cheat samples — synthetic lab markers only.
  register_cheat(make_sig(
      "lab_external_radar_v1", "1.0.0", "external_radar",
      {0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48,
       0x83, 0xEC, 0x40, 0x48, 0x8B, 0xF9, 0xB8, 0xAC, 0x1D, 0x00, 0x00, 0x89,
       0x44, 0x24, 0x20, 0xE8, 0x11, 0x22, 0x33, 0x44}));

  register_cheat(make_sig(
      "lab_aimbot_soft_v2", "2.1.0", "aimbot",
      {0x40, 0x53, 0x48, 0x83, 0xEC, 0x30, 0x0F, 0x29, 0x74, 0x24, 0x20, 0x48,
       0x8B, 0xD9, 0xF3, 0x0F, 0x10, 0x05, 0xDE, 0xAD, 0xBE, 0xEF, 0x0F, 0x28,
       0xF0, 0xE8, 0x55, 0x66, 0x77, 0x88, 0x84, 0xC0}));

  register_cheat(make_sig(
      "lab_overlay_esp_v3", "3.0.1", "overlay",
      {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x08, 0x48, 0x89, 0x70, 0x10, 0x48,
       0x89, 0x78, 0x18, 0x55, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57,
       0x48, 0x8D, 0x68, 0xA1, 0x48, 0x81, 0xEC, 0x90, 0x00, 0x00, 0x00, 0x45,
       0x33, 0xF6, 0xC7, 0x45, 0x00, 0x45, 0x53, 0x50, 0x21}));

  register_cheat(make_sig(
      "lab_dma_bridge_v1", "1.4.2", "dma",
      {0x4C, 0x8B, 0xDC, 0x49, 0x89, 0x5B, 0x08, 0x49, 0x89, 0x6B, 0x10, 0x49,
       0x89, 0x73, 0x18, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57,
       0x48, 0x83, 0xEC, 0x50, 0x45, 0x8B, 0xE1, 0xBA, 0x44, 0x4D, 0x41, 0x00}));

  register_cheat(make_sig(
      "lab_syscall_stub_v1", "1.0.3", "syscall",
      {0x4C, 0x8B, 0xD1, 0xB8, 0x26, 0x00, 0x00, 0x00, 0x0F, 0x05, 0xC3, 0x90,
       0x48, 0x83, 0xEC, 0x28, 0x48, 0x8B, 0x05, 0xAA, 0xBB, 0xCC, 0xDD, 0xFF,
       0x15, 0x00, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC4, 0x28, 0xC3}));

  // Benign control sample — known_cheat=false so match() ignores it.
  register_cheat(make_sig(
      "lab_benign_crt_startup", "msvc", "benign",
      {0x48, 0x83, 0xEC, 0x28, 0xE8, 0x00, 0x00, 0x00, 0x00, 0x48, 0x83, 0xC4,
       0x28, 0xC3, 0xCC, 0xCC},
      false));
}

void CheatSignatureDb::clear() {
  signatures_.clear();
  seeded_ = false;
}

void CheatSignatureDb::register_cheat(const CheatSignature& sig) {
  if (sig.name.empty() || sig.code_sample.empty() || sig.bytes_to_match == 0) {
    return;
  }

  CheatSignature normalized = sig;
  normalized.bytes_to_match =
      std::min(normalized.bytes_to_match, normalized.code_sample.size());
  const auto existing = std::find_if(
      signatures_.begin(), signatures_.end(),
      [&normalized](const CheatSignature& candidate) {
        return candidate.name == normalized.name &&
               candidate.version == normalized.version;
      });
  if (existing == signatures_.end()) {
    signatures_.push_back(std::move(normalized));
  } else {
    *existing = std::move(normalized);
  }
}

int CheatSignatureDb::known_cheat_count() const {
  int n = 0;
  for (const auto& s : signatures_) {
    if (s.known_cheat) ++n;
  }
  return n;
}

std::optional<CheatSignature> CheatSignatureDb::match(
    const std::vector<std::uint8_t>& code_sample, double threshold) const {
  auto ranked = match_all(code_sample, threshold);
  if (ranked.empty()) return std::nullopt;
  return ranked.front().signature;
}

std::vector<CheatMatch> CheatSignatureDb::match_all(
    const std::vector<std::uint8_t>& code_sample, double threshold) const {
  std::vector<CheatMatch> hits;
  if (code_sample.empty() || threshold < 0.0 || threshold > 1.0) return hits;

  for (const auto& signature : signatures_) {
    if (!signature.known_cheat) continue;
    const std::size_t bytes =
        std::min(signature.bytes_to_match, signature.code_sample.size());
    if (bytes == 0 || code_sample.size() < bytes) continue;

    std::vector<std::uint8_t> observed(code_sample.begin(),
                                       code_sample.begin() +
                                           static_cast<std::ptrdiff_t>(bytes));
    std::vector<std::uint8_t> reference(
        signature.code_sample.begin(),
        signature.code_sample.begin() + static_cast<std::ptrdiff_t>(bytes));
    const double sim = compute_similarity(observed, reference);
    if (sim > threshold) {
      CheatMatch m;
      m.signature = signature;
      m.similarity = sim;
      m.compared_bytes = bytes;
      hits.push_back(std::move(m));
    }
  }

  std::sort(hits.begin(), hits.end(),
            [](const CheatMatch& a, const CheatMatch& b) {
              return a.similarity > b.similarity;
            });
  return hits;
}

double CheatSignatureDb::compute_similarity(
    const std::vector<std::uint8_t>& a,
    const std::vector<std::uint8_t>& b) const {
  const std::size_t compared = std::min(a.size(), b.size());
  if (compared == 0) return 0.0;

  std::size_t matches = 0;
  for (std::size_t i = 0; i < compared; ++i) matches += a[i] == b[i] ? 1 : 0;
  return static_cast<double>(matches) / static_cast<double>(compared);
}

}  // namespace lab
