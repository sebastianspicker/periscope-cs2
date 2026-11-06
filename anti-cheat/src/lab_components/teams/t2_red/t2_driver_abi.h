#pragma once

/*
 * T2 driver ABI — documentation of the detection surface only.
 * Not a loadable driver. No implementation of kernel R/W.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define T2_DEVICE_NAME_LAB "\\\\.\\AcLabMemRw"
#define T2_IOCTL_BASE 0x8000u

#ifndef CTL_CODE_LAB
#define CTL_CODE_LAB(n) ((T2_IOCTL_BASE << 16) | ((n) << 2))
#endif

/* IOCTL obfuscation: codes are XOR-encrypted at rest, decrypted at runtime. */
#define T2_IOCTL_XOR_KEY 0xA5A5A5A5u
#define T2_IOCTL_ENC(v)  ((v) ^ T2_IOCTL_XOR_KEY)
#define T2_IOCTL_DEC(v)  ((v) ^ T2_IOCTL_XOR_KEY)

/* Lab IOCTL codes (fictional) — stored encrypted at rest. */
#define T2_IOCTL_PING_ENC       T2_IOCTL_ENC(CTL_CODE_LAB(0x01))
#define T2_IOCTL_READ_VA_ENC    T2_IOCTL_ENC(CTL_CODE_LAB(0x02))
#define T2_IOCTL_FIND_PROC_ENC  T2_IOCTL_ENC(CTL_CODE_LAB(0x03))

typedef struct T2_READ_VA_REQ {
  uint32_t target_pid;
  uint64_t address;
  uint32_t size;
  uint32_t flags;
} T2_READ_VA_REQ;

typedef struct T2_READ_VA_RSP {
  uint32_t status;
  uint32_t size_returned;
  /* bytes follow in real protocols — omitted on purpose */
} T2_READ_VA_RSP;

#ifdef __cplusplus
}
#endif
