#include "blue_example.hpp"
#include <sstream>
namespace examples::schema_saas_product {
BlueResult detect(sim::World& w) {
  BlueResult r;
  int reasons = 0;
  if (w.schema_saas_product) ++reasons;
  if (!w.schema_saas_endpoint.empty()) ++reasons;
  if (w.schema_cache_file_hits >= 1) ++reasons;
  if (w.schema_version_pin) ++reasons;
  bool net = false;
  for (const auto& n : w.net) {
    if (n.looks_like_offset_c2 ||
        n.dest.find("schema") != std::string::npos) {
      net = true;
      break;
    }
  }
  if (net) ++reasons;
  r.detected = reasons >= 2;
  r.mitigated = reasons >= 3;
  if (r.mitigated) {
    w.schema_saas_product = false;
    w.schema_version_pin = false;
    w.ranked_access_denied = true;
  }
  std::ostringstream oss;
  oss << "schema_saas_product blue reasons=" << reasons
      << " net=" << (net ? 1 : 0) << " no_full_compiler=1";
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}
}  // namespace examples::schema_saas_product
