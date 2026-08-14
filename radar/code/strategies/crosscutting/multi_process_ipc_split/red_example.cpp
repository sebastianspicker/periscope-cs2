#include "red_example.hpp"

#include <cstdio>

namespace examples::multi_process_ipc_split {

RedResult apply(sim::World& w) {
  int steps = 0;
  const auto game_pid = w.game_pid();
  const auto* game = w.proc(game_pid);
  if (!game || !game->is_game) {
    std::printf("[T0 multi_process_ipc_split] FAIL: game process is unavailable\n");
    return {false, steps, 0, "precondition failed: game process unavailable"};
  }
  const auto game_base = game->base;
  std::printf("[T0 multi_process_ipc_split] step %d: game pid=%u validated\n", ++steps,
              game_pid);

  const auto holder = w.spawn("audiodg.exe");
  const auto reader = w.spawn("radar-engine.exe");
  const auto ui = w.spawn("radar-ui.exe");
  if (!w.proc(holder) || !w.proc(reader) || !w.proc(ui)) {
    std::printf("[T0 multi_process_ipc_split] FAIL: split process creation was not observable\n");
    return {false, steps, 0, "split process spawn verification failed"};
  }
  std::printf("[T0 multi_process_ipc_split] step %d: holder, reader, and UI spawned\n", ++steps);

  if (!w.open_process(holder, game_pid, sim::AccessMask::VmRead, false)) {
    std::printf("[T0 multi_process_ipc_split] FAIL: holder VM_READ handle denied\n");
    return {false, steps, 0, "holder OpenProcess VM_READ failed"};
  }
  const auto entity_read = w.read_mem(holder, game_pid, game_base + w.lab_entity_table_rel,
                                      sizeof(std::uint32_t), true);
  if (entity_read.status != ac::Status::Ok || entity_read.bytes.empty()) {
    std::printf("[T0 multi_process_ipc_split] FAIL: holder entity read failed\n");
    return {false, steps, 0, "holder entity read failed"};
  }
  std::printf("[T0 multi_process_ipc_split] step %d: holder obtained entity bytes\n", ++steps);

  // The holder forwards its read to the reader; the reader then exposes it to the UI.
  w.add_section({"holder-to-reader-entities", holder, reader, true});
  w.add_section({"reader-to-ui-entities", reader, ui, true});
  w.add_overlay({ui, "radar-ui", true, true, false});

  bool holder_has_handle = false;
  bool reader_has_handle = false;
  bool ui_has_handle = false;
  for (const auto& handle : w.handles_to(game_pid, true)) {
    if (!sim::has(handle.access, sim::AccessMask::VmRead)) continue;
    holder_has_handle = holder_has_handle || handle.owner_pid == holder;
    reader_has_handle = reader_has_handle || handle.owner_pid == reader;
    ui_has_handle = ui_has_handle || handle.owner_pid == ui;
  }
  bool reader_to_ui_section = false;
  for (const auto& section : w.sections) {
    reader_to_ui_section = reader_to_ui_section ||
        (section.creator_pid == reader && section.consumer_pid == ui &&
         section.carries_entity_bytes);
  }
  w.multi_process_split_active = true;
  w.split_holder_pid = holder;
  w.split_reader_pid = reader;
  w.split_ui_pid = ui;
  const bool scar_verified = w.multi_process_split_active && holder_has_handle &&
      !reader_has_handle && !ui_has_handle && reader_to_ui_section;
  if (!scar_verified) {
    std::printf("[T0 multi_process_ipc_split] FAIL: split IPC architecture was not retained\n");
    return {false, steps, 3, "scar verification failed: split IPC architecture"};
  }
  std::printf("[T0 multi_process_ipc_split] step %d: split handles and IPC verified\n", ++steps);
  const std::string detail = "multi_process_ipc_split: holder, reader, and UI split by IPC";
  w.note(detail);
  return {true, steps, 3, detail};
}

}  // namespace examples::multi_process_ipc_split
