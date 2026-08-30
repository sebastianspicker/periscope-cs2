// T1 Duel — hooks go blind; handles do not.

#include "ac/telemetry.hpp"
#include "sim/narrative.hpp"
#include "sim/world.hpp"
#include "t1_blue/hook_trap.hpp"
#include "t1_blue/staging_detector.hpp"
#include "t1_red/crypto_offsets.hpp"
#include "t1_red/syscall_cheat.hpp"

#include <cstdio>
#include <unordered_map>

int main() {
  sim::Narrator n;
  n.say(sim::Side::Lesson,
        "T1: red uses syscall-shaped opens to dodge usermode API hooks.");

  auto world = sim::make_arena();
  ac::MemoryTelemetrySink sink;

  n.move(sim::Side::Red, "Staged loader + encrypted offsets",
         "Business protection + anti-YARA; not anti-handle.");
  t1_red::CryptoOffsets off;
  auto sealed = t1_red::CryptoOffsets::seal({{"entity", 0x10}}, 0x3C);
  off.open(sealed, 0x3C);
  std::printf("    offset entity=0x%llx\n",
              static_cast<unsigned long long>(off.get("entity")));

  t1_red::SyscallCheat red(world);
  red.stage_payload_lab();

  n.move(sim::Side::Red, "NtOpenProcess via direct syscall (sim)",
         "Never call hooked ntdll export.");
  red.attach_via_syscall();
  red.pull_entities();
  std::printf("    entities=%zu\n", red.entities().size());

  auto blindness = t1_red::describe_hook_blindness();
  n.say(sim::Side::Lesson, blindness.lesson);

  std::uint32_t game = 0;
  for (const auto& p : world.list_processes(false)) {
    if (p.is_game) {
      game = p.pid;
    }
  }

  n.counter(sim::Side::Blue, "Usermode hook trap (naive)",
            "Only counts non-syscall path attaches.");
  t1_blue::UsermodeHookTrap hooks;
  const int hook_hits = hooks.count_visible_opens(world, game);
  std::printf("    hook_visible_opens=%d (expect 0)\n", hook_hits);

  n.counter(sim::Side::Blue, "Handle truth monitor (correct)",
            "Object callbacks / handle table — path irrelevant.");
  t1_blue::HandleTruthMonitor truth;
  const int handle_hits = truth.count_vm_read_handles(world, game);
  std::printf("    vm_read_handles=%d (expect >=1)\n", handle_hits);

  n.counter(sim::Side::Blue, "Staging detector",
            "Private RX / stub behavior — supportive only.");
  t1_blue::StagingDetector staging(sink);
  auto sf = staging.inspect(world, red.pid(), red.has_private_rx(), true);
  n.say(sim::Side::Blue, sf.hit ? sf.detail : "no staging signal");

  const bool naive_blue_loses = hook_hits == 0;
  const bool correct_blue_wins = handle_hits > 0;
  n.result(correct_blue_wins,
           naive_blue_loses
               ? "Naive hook-only AC is blind; handle-based AC still wins T1."
               : "Hooks also saw the open (unexpected for syscall path).");

  n.say(sim::Side::Lesson,
        "Next: T2 removes the handle via kernel/BYOVD. Run duel_t2.");
  return correct_blue_wins ? 0 : 1;
}
