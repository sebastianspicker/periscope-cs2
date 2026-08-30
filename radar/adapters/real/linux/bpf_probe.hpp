// bpf_probe.hpp — eBPF / bpffs availability probes (BLUE visibility).
//
// Anti-cheat and EDR increasingly use eBPF. Lab code probes whether
// bpffs is mounted and whether CAP_BPF-style operations are plausible.

#pragma once

#include "real/error.hpp"

#include <string>
#include <vector>

namespace real::linux::bpf {

struct BpfStatus {
    bool bpffs_mounted = false;
    std::string bpffs_path; // typically /sys/fs/bpf
    bool bpf_syscall_known = false;
    std::vector<std::string> pinned_objects; // top-level names under bpffs
    std::vector<std::string> notes;
};

Result<BpfStatus> probe() noexcept;

/// Pure: detect bpffs mount from a /proc/mounts-style text blob.
bool mounts_has_bpffs(const std::string& mounts_text,
                      std::string& path_out) noexcept;

} // namespace real::linux::bpf
