# Linux Real Backend (`lib/real/linux`)

State-of-the-art **educational** Linux userspace + kernel lab stack for
anti-cheat research (T0–T4 peer of the Windows `real/win` + `real/kernel` trees).

## Components

| Module | Role |
|--------|------|
| `memory.*` | `/dev/mem`, `/dev/crash`, pagemap, `process_vm_*`, `/proc/pid/mem`, ptrace, kcore, `finit_module` |
| `process.*` | `/proc` enumeration, maps, threads, fds, namespaces, cgroup, credentials |
| `pcie.*` | sysfs PCI enum, BAR0–5 mmap, config space, port I/O, IOMMU group probes |
| `pagemap.hpp` | Pure pagemap decode + x86-64 page-table index helpers |
| `page_walk.*` | Software 4-level page walk over a physical-read callback |
| `kmod_client.*` | RAII `/dev/aclab` IOCTL client (shared ABI `aclab_ioctl.h`) |
| `aclab_module.c` | Lab kernel module: phys R/W, CR3, virt R/W, DKOM hide, cred steal |
| `aclab_client.cpp` | CLI for the module (`--scan`, `--phys`, `--cr3`, `--virt`) |
| `anti_debug.*` | TracerPid / Yama scope / dumpable sensors |
| `hook_detect.*` | LD_PRELOAD, `ld.so.preload`, suspicious executable maps |
| `module_enum.*` | `/proc/modules` parse + high-risk name denylist |
| `bpf_probe.*` | bpffs mount / pinned object visibility |
| `netlink_audit.*` | audit_enabled / audit_pid sysctl probes |

## Build (userspace)

```bash
# From this directory (host Windows OK for unit tests; Linux for live paths)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

On Linux the same build produces `aclab_client`. Wire into the root project
with `add_subdirectory(lib/real/linux)` when `CMAKE_SYSTEM_NAME` is Linux.

## Build (kernel module)

```bash
make            # builds aclab_module.ko via Kbuild
sudo make load  # insmod + /dev/aclab
sudo make test  # smoke + unload
```

Requires kernel headers matching `uname -r`.

## SCAR / BLUE notes

Every privileged path documents **technique → forensic scar → blue sensor →
mitigation** in the `.cpp` commentary. Nothing here is stealthy by design:
the lab exists to practice **detection**, not to ship cheats.

## Tests

`tests/linux_stack_test.cpp` exercises **shipped** entry points:

- Pure parsers/decoders (always)
- `read_self_memory` content integrity (always)
- `page_walk::walk` with a mock physical backend (always)
- Live `/proc`, `process_vm_readv`, sensors (Linux only)
- Structural asserts on `aclab_module.c` IOCTL surface
