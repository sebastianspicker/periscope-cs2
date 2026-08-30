// byovd_surface.cpp — T2 red kernel/BYOVD path on sim::World drivers/devices/callbacks.
// load_driver/create_device scars

#include "t2_red/byovd_surface.hpp"

namespace t2_red {

// ByovdSurface::set_candidate: Set candidate on this lab unit.
void ByovdSurface::set_candidate(VulnerableDriverRef ref) {
  candidate_ = std::move(ref);
  loaded_ = false;
  last_ = {};
}

// ByovdSurface::attempt_load_lab: Attempt lab BYOVD load + device create scars.
ac::Status ByovdSurface::attempt_load_lab() {
  if (candidate_.image_name.empty() || candidate_.sha256_hex.empty()) {
    return ac::Status::InvalidArgument;
  }
  loaded_ = true;
  last_.loaded = true;
  last_.detail = "lab_mark_only " + candidate_.image_name;
  return ac::Status::Ok;
}

// ByovdSurface::load_on_world: Load BYOVD driver residual onto World.
ByovdLoadReport ByovdSurface::load_on_world(sim::World& w) {
  last_ = {};
  if (candidate_.image_name.empty() || candidate_.sha256_hex.empty()) {
    last_.detail = "empty_candidate";
    return last_;
  }
  // Signed ≠ safe: byovd_known_bad=true is the blue blocklist key surface.
  w.load_driver(sim::Driver{candidate_.image_name, candidate_.sha256_hex,
                            candidate_.signer,
                            /*boot_start=*/false,
                            /*byovd_known_bad=*/true,
                            /*is_ac=*/false,
                            /*is_bridge=*/false,
                            /*provides_mem_rw=*/true,
                            /*load_order=*/10});
  w.create_device(sim::Device{candidate_.device_name, candidate_.image_name,
                              /*mem_rw_ioctl=*/true});
  // SCM-shaped scar (service start).
  w.add_service(sim::ServiceEvent{candidate_.image_name + "_svc",
                                  candidate_.image_name, true});
  loaded_ = true;
  last_.loaded = true;
  last_.device_created = true;
  last_.detail = "byovd " + candidate_.image_name + " sha=" +
                 candidate_.sha256_hex + " device=" + candidate_.device_name;
  w.note("t2 ByovdSurface " + last_.detail);
  return last_;
}

}  // namespace t2_red
