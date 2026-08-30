// host_sim_fpga.cpp — FpgaDevice (see host_sim_fpga.hpp).
#include "host_sim_fpga.hpp"

#include <cstdarg>
#include <cstdio>

static bool g_quiet = false;

bool host_sim_quiet() { return g_quiet; }
void host_sim_set_quiet(bool q) { g_quiet = q; }

void host_sim_log(const char* tag, const char* fmt, ...) {
    if (g_quiet) return;
    std::fprintf(stderr, "[%-8s] ", tag);
    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(stderr, fmt, ap);
    va_end(ap);
    std::fputc('\n', stderr);
}

FpgaDevice* FpgaDevice::s_inst = nullptr;
