#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "52_schema_saas_product", "Red: schema SaaS endpoint + cache-file hits + version pin product");
  const auto red = examples::schema_saas_product::apply(w);
  n.say(sim::Side::Red, red.detail);
  n.counter(sim::Side::Blue, "52_schema_saas_product", "Blue: schema-as-product residual without full schema compiler");
  const auto blue = examples::schema_saas_product::detect(w);
  n.say(sim::Side::Blue, blue.detail);
  StrategyResult r;
  r.red_achieved = red.achieved;
  r.blue_detected = blue.detected;
  r.blue_mitigated = blue.mitigated;
  r.summary = red.detail + " | " + blue.detail;
  n.result(r.blue_detected || r.blue_mitigated, r.summary);
  return r;
}

}  // namespace

StrategyEntry entry_52_schema_saas_product() {
  return {{ "52_schema_saas_product", "schema saas product", Family::Evasion, "all",
           "Red: schema SaaS endpoint + cache-file hits + version pin product",
           "Blue: schema-as-product residual without full schema compiler"},
          run};
}
}  // namespace strategies
