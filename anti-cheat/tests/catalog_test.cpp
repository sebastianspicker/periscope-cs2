// catalog_test.cpp - registry integrity and full catalog smoke run.

#include "strategies/catalog_util.hpp"
#include "strategies/framework.hpp"

#include <cstdio>
#include <string>

int main() {
  const auto& cat = strategies::catalog();
  const std::string problems = strategies::validate_catalog(cat);
  if (!problems.empty()) {
    std::fprintf(stderr, "catalog validation failed:\n%s", problems.c_str());
    return 1;
  }
  if (cat.empty()) {
    std::fprintf(stderr, "catalog is empty\n");
    return 1;
  }
  // Two pairs with the same id are the most common merge accident; the
  // validator catches it, this is a second guard with a clear message.
  for (std::size_t i = 0; i < cat.size(); ++i) {
    for (std::size_t j = i + 1; j < cat.size(); ++j) {
      if (std::string(cat[i].meta.id) == cat[j].meta.id) {
        std::fprintf(stderr, "duplicate id: %s\n", cat[i].meta.id);
        return 1;
      }
    }
  }
  std::printf("catalog valid: %zu entries\n", cat.size());
  return 0;
}
