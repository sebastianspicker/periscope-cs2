#include <cstdio>
#include <windows.h>
#include "real/win/api_table.hpp"
#include "real/cs2/process.hpp"
#include "real/process.hpp"

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  printf("1 g_Api\n");
  auto& api = real::win::g_Api();
  printf("2 resolved=%d OpenProcess=%p CreateToolhelp=%p\n",
         api.resolved, (void*)api.OpenProcess, (void*)api.CreateToolhelp32Snapshot);
  printf("3 enum_processes\n");
  auto procs = real::enum_processes();
  printf("4 ok=%d count=%zu err=%s\n", (bool)procs,
         procs ? procs->size() : 0,
         procs ? "-" : procs.error_msg.c_str());
  if (procs) {
    for (auto& p : *procs) {
      if (p.name.find("cs2") != std::string::npos || p.name.find("CS2") != std::string::npos)
        printf("  found %s pid=%u\n", p.name.c_str(), p.pid);
    }
  }
  printf("5 find_cs2\n");
  auto cs2 = real::cs2::find_cs2_process();
  printf("6 find ok=%d\n", (bool)cs2);
  if (cs2) printf("  pid=%u name=%s\n", cs2->pid, cs2->name.c_str());
  else printf("  err=%s\n", cs2.error_msg.c_str());
  printf("7 attach\n");
  auto a = real::cs2::attach_to_cs2(1);
  printf("8 attached=%d err=%s pid=%u base=%llx\n", a.attached, a.error_msg.c_str(), a.pid, (unsigned long long)a.base_address);
  return 0;
}
