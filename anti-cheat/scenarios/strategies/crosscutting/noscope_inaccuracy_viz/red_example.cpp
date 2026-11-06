#include "red_example.hpp"
#include <sstream>
namespace examples::noscope_inaccuracy_viz {
RedResult apply(sim::World& w) {
  RedResult r;
  r.actor_pid = w.spawn("noscope-viz.exe");
  w.noscope_inaccuracy_viz = true;
  w.noscope_spread_shown = 0.42f;  // shown spread ring product
  r.achieved = w.noscope_inaccuracy_viz && w.noscope_spread_shown > 0.1f;
  std::ostringstream oss;
  oss << "noscope_inaccuracy_viz red spread_shown=" << w.noscope_spread_shown;
  r.detail = oss.str();
  w.note(r.detail);
  return r;
}
}  // namespace examples::noscope_inaccuracy_viz
