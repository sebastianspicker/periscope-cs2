#pragma once

// Educational simulation: cheat signature database.
// Blue matches thread code samples against known patterns without inspecting
// live process memory or invoking host OS APIs.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lab {

struct CheatSignature {
  std::string name;
  std::vector<std::uint8_t> code_sample;  // First N bytes of thread start / .text
  std::size_t bytes_to_match = 0;
  std::string version;
  std::string family;  // e.g. "external_radar", "aimbot", "overlay"
  bool known_cheat = false;
};

struct CheatMatch {
  CheatSignature signature;
  double similarity = 0.0;
  std::size_t compared_bytes = 0;
};

class CheatSignatureDb {
 public:
  static CheatSignatureDb& instance();

  void register_cheat(const CheatSignature& sig);
  void clear();
  void seed_known_cheats();  // educational corpus (idempotent by name+version)

  /// Match a code sample against known cheat DB.
  /// Returns best match if similarity > threshold.
  std::optional<CheatSignature> match(
      const std::vector<std::uint8_t>& code_sample,
      double threshold = 0.95) const;

  /// Ranked matches above threshold (descending similarity).
  std::vector<CheatMatch> match_all(
      const std::vector<std::uint8_t>& code_sample,
      double threshold = 0.80) const;

  int known_cheats() const { return static_cast<int>(signatures_.size()); }
  int known_cheat_count() const;  // only known_cheat == true
  const std::vector<CheatSignature>& signatures() const { return signatures_; }

  double compute_similarity(const std::vector<std::uint8_t>& a,
                            const std::vector<std::uint8_t>& b) const;

 private:
  std::vector<CheatSignature> signatures_;
  bool seeded_ = false;
  CheatSignatureDb();

  CheatSignature make_sig(std::string_view name, std::string_view version,
                          std::string_view family,
                          std::initializer_list<std::uint8_t> bytes,
                          bool known = true) const;
};

}  // namespace lab
