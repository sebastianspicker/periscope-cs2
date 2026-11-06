#include "red_example.hpp"

#include <cstdio>

namespace strategy::t1_bsecure_allowed_evade {

void Red::apply(sim::World& w) noexcept {
  std::printf("[red:bsecure_allowed_evade] step 1: validate game arena\n");
  const auto game = w.game_pid();
  auto* g = w.proc(game);
  if (game == 0 || g == nullptr) {
    w.note("bsecure_allowed_evade: precondition failed — game missing");
    return;
  }

  std::printf("[red:bsecure_allowed_evade] step 2: spawn stomper actor\n");
  const auto actor = w.spawn("bsecure-stomp-loader.exe");
  if (actor == 0 || w.proc(actor) == nullptr) {
    w.note("bsecure_allowed_evade: actor spawn failed");
    return;
  }

  std::printf("[red:bsecure_allowed_evade] step 3: locate legitimate signed module surface\n");
  if (g->modules.empty()) {
    g->modules.push_back({"client.dll", g->base, 0x10000, true, false, "clean"});
  }
  auto& host = g->modules.front();
  const auto orig_size = host.size;
  const auto orig_hash = host.text_hash;

  std::printf("[red:bsecure_allowed_evade] step 4: stomp host module (inherit signature façade)\n");
  // World-level BSecureAllowed scars
  w.module_loaded = true;
  w.module_is_stomped = true;
  w.module_is_signed = true;  // inherits legit signature
  w.module_signature_date = 0x66800000;
  w.module_timestamp = 0x66800000;  // matches signature date (pass timestamp)
  w.module_size = static_cast<std::uint32_t>(orig_size ? orig_size : 0x10000);
  w.expected_module_size = w.module_size;
  // Integrity residual on host still detectable
  host.text_hash = "stomped";
  host.headers_erased = true;

  std::printf("[red:bsecure_allowed_evade] step 5: verify stomp residual planted\n");
  if (!(w.module_loaded && w.module_is_stomped && w.module_is_signed &&
        host.text_hash == "stomped")) {
    w.note("bsecure_allowed_evade: post-condition failed");
    return;
  }

  w.note("bsecure_allowed_evade: stomped module inherits signature; integrity residual remains");
  std::printf("[red:bsecure_allowed_evade] achieved: loaded=1 stomped=1 signed=1 (host was %s)\n",
              orig_hash.c_str());
}

}  // namespace strategy::t1_bsecure_allowed_evade
