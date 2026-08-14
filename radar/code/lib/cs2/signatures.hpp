#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace cs2 {

// A single CS2 function signature pattern.
struct SignaturePattern {
  std::string_view name;        // Function name (for example, "ACCEPTINPUT").
  std::string_view bytes_hex;   // Raw hex bytes with ?? for wildcards.
  std::string_view category;    // Category tag.
  const char* description;      // Brief description.
};

inline constexpr std::string_view CAT_RENDER{"render", 6};
inline constexpr std::string_view CAT_ENTITY{"entity", 6};
inline constexpr std::string_view CAT_PLAYER{"player", 6};
inline constexpr std::string_view CAT_WEAPON{"weapon", 6};
inline constexpr std::string_view CAT_INPUT{"input", 5};
inline constexpr std::string_view CAT_NET{"network", 7};
inline constexpr std::string_view CAT_MOVE{"movement", 8};
inline constexpr std::string_view CAT_GAMESTATE{"gamestate", 9};
inline constexpr std::string_view CAT_SOUND{"sound", 5};
inline constexpr std::string_view CAT_GLOW{"glow", 4};
inline constexpr std::string_view CAT_SYSTEM{"system", 6};
inline constexpr std::string_view CAT_UI{"ui", 2};
inline constexpr std::string_view CAT_TRACE{"trace", 5};
inline constexpr std::string_view CAT_ECON{"econ", 4};

// All signatures organized by category.
struct SignatureCategory {
  std::string_view name;
  int count;
  const SignaturePattern* patterns;
};

// Master registry for the CS2 signature corpus.
struct SignatureDatabase {
  static constexpr int CATEGORY_COUNT = 14;
  SignatureCategory categories[CATEGORY_COUNT];

  static const SignatureDatabase& get();

  // Lookup returns the first entry when the corpus contains duplicate names.
  const SignaturePattern* find(std::string_view name) const;
  const SignatureCategory* category(std::string_view cat_name) const;
  int total_count() const;
};

// Pattern scanner simulation -- models what real cheat engines do.
struct PatternScanResult {
  std::string_view name;
  bool found;
  std::uint64_t resolved_address;
  std::uint64_t offset_from_base;
};

class PatternScanner {
 public:
  std::vector<PatternScanResult> scan_all(std::uint64_t game_memory_base,
                                           std::size_t game_memory_size,
                                           const std::uint8_t* game_memory);
  PatternScanResult scan_one(std::string_view name,
                              std::uint64_t game_memory_base,
                              std::size_t game_memory_size,
                              const std::uint8_t* game_memory);

  const SignatureDatabase& db() const { return db_; }

 private:
  SignatureDatabase db_{SignatureDatabase::get()};

  bool match_pattern(const std::uint8_t* memory, std::size_t size,
                     std::size_t offset, std::string_view pattern_hex) const;

  struct ParsedPattern {
    std::vector<std::uint8_t> bytes;
    std::vector<bool> mask;  // true = compare, false = wildcard
  };
  ParsedPattern parse_pattern(std::string_view hex) const;
};

}  // namespace cs2
