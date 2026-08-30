/* aclab_ioctl.h — Shared ABI between aclab_module.ko and userspace clients.
 *
 * Educational T2/T3 kernel lab interface. Every IOCTL leaves forensic scars
 * in dmesg, audit logs, and /proc/modules. Used for anti-cheat research only.
 *
 * C ABI — safe for both kernel module and C++ userspace.
 */
#ifndef REAL_LINUX_ACLAB_IOCTL_H
#define REAL_LINUX_ACLAB_IOCTL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Userspace needs linux/ioctl.h or sys/ioctl.h; kernel provides its own. */
#if defined(__KERNEL__)
/* kernel: ioctl macros already available via linux headers */
#elif defined(_WIN32)
/* Host-side unit tests: pure macro definitions without kernel headers. */
#ifndef _IOC
#define _IOC(dir, type, nr, size) \
  (((dir) << 30) | ((type) << 8) | (nr) | ((size) << 16))
#define _IOC_WRITE 1U
#define _IOC_READ  2U
#define _IOW(type, nr, size)  _IOC(_IOC_WRITE, (type), (nr), (unsigned)sizeof(size))
#define _IOWR(type, nr, size) _IOC(_IOC_READ | _IOC_WRITE, (type), (nr), (unsigned)sizeof(size))
#endif
#else
#include <sys/ioctl.h>
#include <sys/types.h>
#endif

#ifndef __KERNEL__
/* Ensure pid_t exists for userspace / host unit-test builds. */
#if defined(_WIN32)
typedef int pid_t;
#endif
#endif

#define ACLAB_DEV_NAME      "aclab"
#define ACLAB_CLASS_NAME    "aclab_class"
#define ACLAB_DEV_PATH      "/dev/aclab"
#define ACLAB_MODULE_NAME   "aclab_module"
#define ACLAB_MAJOR_DEFAULT 242
#define ACLAB_MINOR_DEFAULT 0
#define ACLAB_MAX_XFER      4096u

#define ACLAB_IOCTL_BASE 'A'

#pragma pack(push, 1)

struct aclab_phys_op {
    uint64_t phys_addr;
    uint32_t size;
    uint8_t  data[ACLAB_MAX_XFER];
};

struct aclab_cr3_req {
    pid_t    pid;
    uint64_t cr3;
};

struct aclab_virt_op {
    pid_t    pid;
    uint64_t virt_addr;
    uint32_t size;
    uint8_t  data[ACLAB_MAX_XFER];
};

struct aclab_cred_steal {
    pid_t target_pid;
    pid_t source_pid; /* typically 1 (init) for root creds */
};

struct aclab_mod_info {
    uint32_t major;
    uint32_t minor;
    uint32_t abi_version;
    char     build_tag[32];
};

#pragma pack(pop)

#define ACLAB_ABI_VERSION 2u

#define ACLAB_IOCTL_PHYS_READ  _IOWR(ACLAB_IOCTL_BASE, 1, struct aclab_phys_op)
#define ACLAB_IOCTL_PHYS_WRITE _IOWR(ACLAB_IOCTL_BASE, 2, struct aclab_phys_op)
#define ACLAB_IOCTL_GET_CR3    _IOWR(ACLAB_IOCTL_BASE, 3, struct aclab_cr3_req)
#define ACLAB_IOCTL_VIRT_READ  _IOWR(ACLAB_IOCTL_BASE, 4, struct aclab_virt_op)
#define ACLAB_IOCTL_VIRT_WRITE _IOWR(ACLAB_IOCTL_BASE, 5, struct aclab_virt_op)
#define ACLAB_IOCTL_HIDE_PROC  _IOW(ACLAB_IOCTL_BASE, 6, pid_t)
#define ACLAB_IOCTL_STEAL_CRED _IOW(ACLAB_IOCTL_BASE, 7, struct aclab_cred_steal)
#define ACLAB_IOCTL_MOD_INFO   _IOWR(ACLAB_IOCTL_BASE, 8, struct aclab_mod_info)

/* Pure helpers for host-side unit tests (no syscalls). */
static inline int aclab_phys_op_valid(const struct aclab_phys_op* op) {
    return op && op->size > 0 && op->size <= ACLAB_MAX_XFER;
}

static inline int aclab_virt_op_valid(const struct aclab_virt_op* op) {
    return op && op->pid > 0 && op->size > 0 && op->size <= ACLAB_MAX_XFER;
}

static inline uint32_t aclab_ioctl_nr(unsigned long cmd) {
    return (uint32_t)((cmd >> 0) & 0xFFu);
}

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* REAL_LINUX_ACLAB_IOCTL_H */
