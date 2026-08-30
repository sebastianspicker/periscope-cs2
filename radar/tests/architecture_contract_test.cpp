#include "strategies/framework.hpp"
#include "teams/t2_red/t2_driver_abi.h"

#include <cstdio>

int main() {
  int failures = 0;
  const auto expect = [&failures](bool condition, const char* message) {
    if (!condition) {
      std::fprintf(stderr, "FAIL: %s\n", message);
      ++failures;
    }
  };

  expect(T2_IOCTL_DEC(T2_IOCTL_PING_ENC) == CTL_CODE_LAB(0x01),
         "T2 ping ABI encoding round-trips");
  expect(T2_IOCTL_DEC(T2_IOCTL_READ_VA_ENC) == CTL_CODE_LAB(0x02),
         "T2 read ABI encoding round-trips");
  expect(T2_IOCTL_DEC(T2_IOCTL_FIND_PROC_ENC) == CTL_CODE_LAB(0x03),
         "T2 process ABI encoding round-trips");
  expect(!strategies::catalog().empty(), "strategy catalog is populated");
  return failures;
}
