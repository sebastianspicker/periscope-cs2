#include "strategies/pair_util.hpp"
#include "blue_example.hpp"
#include "red_example.hpp"

namespace strategies {
namespace {

StrategyResult run(sim::World& w, sim::Narrator& n) {
  n.move(sim::Side::Red, "51_dll_thread_protect", "Red: DLL + thread protection detail (find-target, protected threads)");
  const auto red = examples::dll_thread_protect::apply(w);
  n.say(sim::Side::Red, red.detail);
  n.counter(sim::Side::Blue, "51_dll_thread_protect", "Blue: protector depth residual finer than suite flags alone (45)");
  const auto blue = examples::dll_thread_protect::detect(w);
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

StrategyEntry entry_51_dll_thread_protect() {
  return {{ "51_dll_thread_protect", "dll thread protect", Family::Evasion, "all",
           "Red: DLL + thread protection detail (find-target, protected threads)",
           "Blue: protector depth residual finer than suite flags alone (45)"},
          run};
}
}  // namespace strategies
