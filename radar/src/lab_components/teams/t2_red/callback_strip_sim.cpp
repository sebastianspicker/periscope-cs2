// callback_strip_sim.cpp — T2 red kernel/BYOVD path on sim::World drivers/devices/callbacks.
// load_driver/create_device scars

#include "t2_red/callback_strip_sim.hpp"

namespace t2_red {

// CallbackStripSim::strip_aggressive: Red: clear AC callback presence aggressively.
CallbackState CallbackStripSim::strip_aggressive(CallbackState in) {
  in.process_notify = 1;
  in.image_notify = 1;
  in.ac_callback_present = false;
  return in;
}

// CallbackStripSim::capture: Red: capture callback state before strip.
CallbackState CallbackStripSim::capture(const sim::World& w) {
  CallbackState s;
  s.process_notify = w.process_notify;
  s.image_notify = w.image_notify;
  s.ac_callback_present = w.ac_callback_present;
  return s;
}

// CallbackStripSim::strip_world: Red: apply callback strip to World flags.
CallbackStripReport CallbackStripSim::strip_world(sim::World& w) {
  CallbackStripReport r;
  r.before = capture(w);
  w.process_notify = 1;
  w.image_notify = 1;
  w.ac_callback_present = false;
  // Also strip object/minifilter/registry if present (aggressive pack narrative).
  w.object_callbacks_present = false;
  w.object_callbacks = 0;
  w.minifilter_present = false;
  w.minifilter_callbacks = 0;
  w.registry_notify_present = false;
  w.registry_notify = 0;
  r.after = capture(w);
  r.stripped = true;
  r.detail = "strip process_notify=" + std::to_string(r.after.process_notify) +
             " image_notify=" + std::to_string(r.after.image_notify) +
             " ac_cb=0";
  w.note("t2 CallbackStripSim " + r.detail);
  return r;
}

// CallbackStripSim::enable_shadow: Red: enable callback shadow residual.
CallbackStripReport CallbackStripSim::enable_shadow(sim::World& w, bool on) {
  CallbackStripReport r;
  r.before = capture(w);
  w.enable_callback_shadow(on);
  r.shadow = on;
  r.after = capture(w);
  r.detail = std::string("callback_shadow=") + (on ? "on" : "off");
  w.note("t2 CallbackStripSim " + r.detail);
  return r;
}

}  // namespace t2_red
