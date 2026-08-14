#pragma once
// world_api.hpp — sim::World method declarations (included inside struct World).

  void note(std::string line);
  std::uint32_t spawn(std::string name, bool game = false, bool ac = false,
                      std::uint32_t parent_pid = 0);
  Process* proc(std::uint32_t pid);
  const Process* proc(std::uint32_t pid) const;
  std::uint32_t game_pid() const;
  std::uint32_t ac_pid() const;

  // Seed entity table bytes + ACPT pattern marker for RPM/pattern-scan demos.
  void plant_lab_entities(std::uint32_t game_pid);
  // Move/rewrite lab AOB marker and/or entity table offset; bumps lab_pattern_generation.
  // Used by refresh scenarios so red cannot keep a stale resolved VA.
  bool mutate_lab_pattern_layout(std::uint32_t game_pid, std::size_t new_marker_off,
                                 std::uint32_t new_table_rel);
  // Advance lab match sample; optional weak_delta adds to lab_confidence.
  void advance_match_tick(double weak_delta = 0.0);
  // Structural fog+: cut client fidelity and enemy replication budget.
  void apply_client_fidelity_budget(float fidelity, int enemy_budget);
  // Simulated ReadProcessMemory through a handle (or fail without rights).
  ac::ReadResult read_mem(std::uint32_t reader_pid, std::uint32_t target_pid,
                          std::uint64_t addr, std::size_t size,
                          bool require_handle);

  // Record an OpenProcess-style handle edge (owner → target, access mask) on World.
  bool open_process(std::uint32_t owner, std::uint32_t target, AccessMask access,
                    bool syscall_path = false);
  void close_handles_from(std::uint32_t owner);

  /// Record a system handle-table entry (does not itself create World.handles edge).
  void record_handle_table(std::uint32_t owner, std::uint32_t target,
                           std::uint32_t access_mask,
                           ac::HandleAcquisitionModel acquisition,
                           bool ephemeral = false, bool via_proxy = false,
                           bool hidden_during_enum = false);

  /// Write |size| bytes into target process image-relative memory (lab mutator).
  bool write_mem(std::uint32_t target_pid, std::uint64_t addr,
                 const void* data, std::size_t size);

  /// Plant an entity table from snapshots into game memory (fixture path).
  bool plant_entity_snapshots(std::uint32_t game_pid,
                              const std::vector<ac::EntitySnapshot>& entities,
                              std::uint32_t table_rel = 0);

  /// Decode planted entity table from game memory into snapshots.
  std::vector<ac::EntitySnapshot> read_entity_snapshots(
      std::uint32_t game_pid, std::uint32_t table_rel = 0) const;

  // Load a Driver scar into World.drivers.
  void load_driver(Driver d);
  // Create a Device node (often mem_rw_ioctl) linked to a driver image.
  void create_device(Device d);
  // Simulated device IOCTL memory read without a game usermode VM_READ handle.
  bool device_ioctl_read(std::uint32_t opener_pid, const std::string& device,
                         std::uint32_t target_pid, std::uint64_t addr,
                         std::size_t size, std::vector<std::uint8_t>& out);

  bool try_start_personal_hv(std::string vendor);
  // Simulated personal-HV bridge read path on HostTrust.
  bool hv_read(std::uint32_t bridge_pid, std::uint32_t target_pid,
               std::uint64_t addr, std::size_t size,
               std::vector<std::uint8_t>& out);

  /// Off-box PCIe DMA-style read: no local reader process, no game handle.
  /// Succeeds only when a DMA device is present and IOMMU is off, unless the
  /// simulation's explicit IOMMU-bypass scar is active.
  bool dma_read(std::uint32_t target_pid, std::uint64_t addr, std::size_t size,
                std::vector<std::uint8_t>& out);

  // Extended red/blue surfaces
  bool inject_module(std::uint32_t into_pid, Module m, bool manual_map);
  void add_overlay(OverlayWindow o);
  void push_input(InputEvent e);
  void spoof_hwid(std::string new_hwid);
  void add_account(Account a);
  void add_net(NetFlow f);
  void add_service(ServiceEvent s);
  void add_section(SharedSection s);

  /// Snapshot notify counts (applies shadow if red is shadowing).
  void sample_callbacks(std::size_t& out_pn, std::size_t& out_in,
                        bool& out_ac) const;
  void enable_callback_shadow(bool on);

  /// Handle graph as AC sees it (skips hidden_during_enum when sampling).
  std::vector<Handle> handles_to(std::uint32_t target,
                                 bool include_hidden = true) const;
  /// Continuous sample: also count handles that were hidden on last enum.
  int count_hidden_handles_to(std::uint32_t target) const;

  /// Returns handles targeting `pid` (alias of handles_to for readability).
  std::vector<Handle> handles_for(std::uint32_t pid) const;
  /// Removes handles in inherited_handles that belong to `pid`.
  void clear_inherited_handles(std::uint32_t pid);

  std::vector<Process> list_processes(bool weak_enum) const;
