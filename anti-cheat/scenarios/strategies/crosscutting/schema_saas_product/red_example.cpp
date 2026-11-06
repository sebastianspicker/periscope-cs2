#include "red_example.hpp"
#include <sstream>
namespace examples::schema_saas_product {
RedResult apply(sim::World& w) {
  RedResult r;
  r.actor_pid = w.spawn("schema-client.exe");
  w.schema_saas_product = true;
  w.schema_saas_endpoint = "https://schema.valthrun-like.example/v1/cs2";
  w.schema_cache_file_hits = 3;
  w.schema_version_pin = true;
  w.schema_cache_active = true;
  w.schema_cache_version = "saas-pin:build-xyz";
  w.add_net(sim::NetFlow{r.actor_pid, "schema.valthrun-like.example:443",
                         /*looks_like_offset_c2=*/true, false});
  // Not a full AOB walk product — SaaS layout product residual.
  r.achieved = w.schema_saas_product && !w.schema_saas_endpoint.empty() &&
               w.schema_cache_file_hits >= 1 && w.schema_version_pin;
  std::ostringstream oss;
  oss << "schema_saas_product red endpoint=" << w.schema_saas_endpoint
      << " cache_file_hits=" << w.schema_cache_file_hits
      << " version_pin=1 (not full compiler)";
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}
}  // namespace examples::schema_saas_product
