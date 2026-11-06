#include "red_example.hpp"

#include <cstdio>

namespace examples::dual_view_shadow_pages {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    std::printf("[T3 dual_view_shadow_pages] FAIL: game process unavailable\n");
    return {false, steps, "precondition failed: game process unavailable"};
  }
  std::printf("[T3 dual_view_shadow_pages] step %d: game pid=%u validated\n", ++steps, game_pid);

  // Step: the ept_hide_ac base posture (personal HV, hidden AC pages) is the
  // foundation this pair extends - see 48_ept_hide_ac for that lesson alone.
  w.trust.personal_hv_active = true;
  w.trust.ept_hide_ac_pages = true;
  if (!(w.trust.personal_hv_active && w.trust.ept_hide_ac_pages)) {
    return {false, steps, "personal-HV EPT posture was not established"};
  }
  std::printf("[T3 dual_view_shadow_pages] step %d: personal HV + EPT posture established\n", ++steps);

  // Step: split the views. The execute view serves the patched code; the read
  // view keeps serving pristine bytes. This is more than EPT hiding: the same
  // guest page is divergent between fetches and data reads.
  w.dual_view_shadow_active = true;
  w.dual_view_exec_patched = true;
  w.dual_view_read_pristine = true;
  if (!(w.dual_view_shadow_active && w.dual_view_exec_patched &&
        w.dual_view_read_pristine)) {
    return {false, steps, "dual-view split was not established"};
  }
  std::printf("[T3 dual_view_shadow_pages] step %d: exec view patched / read view pristine\n", ++steps);

  // Step: red's own self-check runs through the read view and must return
  // pristine bytes - the guest sees one consistent (pristine) story.
  ++w.dual_view_cross_reads;
  const sim::Module* client = nullptr;
  for (const auto& module : game->modules) {
    if (module.name == "client.dll") client = &module;
  }
  if (client == nullptr || client->text_hash != "clean") {
    std::printf("[T3 dual_view_shadow_pages] FAIL: read view no longer pristine\n");
    return {false, steps, "read-view self-check failed"};
  }
  std::printf("[T3 dual_view_shadow_pages] step %d: guest self-read returns pristine bytes\n", ++steps);

  // Step: verify the split is real - the patch executes (exec view) while
  // every data read (read view) reports the original bytes.
  if (!(w.dual_view_exec_patched && client->text_hash == "clean" &&
        w.dual_view_cross_reads > 0)) {
    return {false, steps, "dual-view verification failed"};
  }
  std::printf("[T3 dual_view_shadow_pages] step %d: dual-view split verified (exec patched, read pristine)\n",
              ++steps);

  const std::string detail =
      "dual_view_shadow_pages: exec_patched=1 read_pristine=1 cross_reads=" +
      std::to_string(w.dual_view_cross_reads) + " guest_hash=clean";
  w.note(detail);
  return {true, steps, detail};
}

}  // namespace examples::dual_view_shadow_pages
