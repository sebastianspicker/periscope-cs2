#pragma once

/*
 * T3 hypercall ABI — detection surface documentation only.
 * No VMX/SVM implementation, no EPT, no guest memory walk.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum T3_HC {
  T3_HC_PING = 1,
  T3_HC_GET_CR3 = 2,
  T3_HC_READ_VA = 3,
  T3_HC_WRITE_VA = 4, /* often present even in "radar only" packs */
  T3_HC_FIND_PROCESS = 5,
} T3_HC;

typedef struct T3_HC_READ_VA_REQ {
  uint64_t cr3;
  uint64_t va;
  uint32_t size;
  uint32_t flags;
} T3_HC_READ_VA_REQ;

#define T3_BRIDGE_DEVICE_LAB "\\\\.\\AcLabHvComm"

#ifdef __cplusplus
}
#endif
