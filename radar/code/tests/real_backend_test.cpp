#include "real/real_fwd.hpp"
#include "ac/types.hpp"

#include <cstdio>

namespace {
int failures = 0;
void expect(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++failures;
  }
}

void test_real_dma_backend() {
#if defined(LR_HAS_REAL_DMA)
  real::dma::RealDmaBackend backend(false);
  expect(!backend.is_attached(), "DMA backend starts detached");
  const auto status = backend.attach(0);
  expect(status == ac::Status::Unavailable || backend.is_attached(),
         "DMA attach denies gracefully without hardware");
  if (!backend.is_attached()) {
    expect(backend.read({0x1000, 32}).status == ac::Status::Denied,
           "detached DMA read is denied");
  }
  backend.detach();
  expect(!backend.is_attached(), "DMA detach is safe");
#else
  expect(true, "DMA backend is not enabled");
#endif
}

void test_platform_api() {
  expect(!real::format_os_error(0).empty(), "OS error formatting is available");
  expect(!real::read_physical(0, 4096).ok,
         "physical read is denied gracefully without privileges");
}
}  // namespace

int main() {
  test_real_dma_backend();
  test_platform_api();
  return failures;
}
