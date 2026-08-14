#include "real/library.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

#if LR_PLATFORM_WINDOWS
#include "real/win/api_table.hpp"
#include "real/win/windows_h.hpp"
#elif LR_PLATFORM_LINUX
#include <dlfcn.h>
#elif defined(__APPLE__)
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#endif

namespace real {
namespace {

bool module_name_matches(const std::string& path, const std::string& requested) {
  if (path == requested) return true;
  const auto separator = path.find_last_of('/');
  return path.substr(separator == std::string::npos ? 0 : separator + 1) == requested;
}

#if LR_PLATFORM_LINUX
Result<std::pair<std::uint64_t, std::uint64_t>> module_range(const std::string& name) {
  std::ifstream maps("/proc/self/maps");
  std::string line;
  std::uint64_t begin = std::numeric_limits<std::uint64_t>::max();
  std::uint64_t end = 0;
  while (std::getline(maps, line)) {
    const auto dash = line.find('-');
    const auto first_space = line.find(' ');
    if (dash == std::string::npos || first_space == std::string::npos) continue;
    const auto path_start = line.find('/', first_space);
    if (path_start == std::string::npos) continue;
    const std::string path = line.substr(path_start);
    if (!module_name_matches(path, name)) continue;
    try {
      begin = std::min(begin, std::stoull(line.substr(0, dash), nullptr, 16));
      end = std::max(end, std::stoull(line.substr(dash + 1, first_space - dash - 1), nullptr, 16));
    } catch (const std::exception&) {
      return {{}, "Malformed /proc/self/maps entry"};
    }
  }
  if (end <= begin) return {{}, "Module not found: " + name};
  return Result<std::pair<std::uint64_t, std::uint64_t>>({begin, end});
}
#endif

#if defined(__APPLE__)
Result<std::pair<std::uint64_t, std::uint64_t>> module_range(const std::string& name) {
  for (std::uint32_t index = 0; index < _dyld_image_count(); ++index) {
    const char* image_name = _dyld_get_image_name(index);
    if (image_name == nullptr || !module_name_matches(image_name, name)) continue;

    const mach_header* header = _dyld_get_image_header(index);
    if (header == nullptr) return {{}, "Loaded image has no Mach-O header: " + name};

    const auto slide = _dyld_get_image_vmaddr_slide(index);
    std::uint64_t begin = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t end = 0;
    const bool is_64_bit = header->magic == MH_MAGIC_64 || header->magic == MH_CIGAM_64;
    const load_command* command = reinterpret_cast<const load_command*>(
        reinterpret_cast<const std::uint8_t*>(header) +
        (is_64_bit ? sizeof(mach_header_64) : sizeof(mach_header)));

    for (std::uint32_t command_index = 0; command_index < header->ncmds; ++command_index) {
      if (command->cmdsize < sizeof(load_command)) {
        return {{}, "Malformed Mach-O load command: " + name};
      }
      if (command->cmd == LC_SEGMENT) {
        const auto* segment = reinterpret_cast<const segment_command*>(command);
        begin = std::min(begin, static_cast<std::uint64_t>(segment->vmaddr));
        end = std::max(end, static_cast<std::uint64_t>(segment->vmaddr) + segment->vmsize);
      } else if (command->cmd == LC_SEGMENT_64) {
        const auto* segment = reinterpret_cast<const segment_command_64*>(command);
        begin = std::min(begin, segment->vmaddr);
        end = std::max(end, segment->vmaddr + segment->vmsize);
      }
      command = reinterpret_cast<const load_command*>(
          reinterpret_cast<const std::uint8_t*>(command) + command->cmdsize);
    }

    if (end <= begin) return {{}, "Mach-O image has no mapped segments: " + name};
    const auto loaded_begin = static_cast<std::int64_t>(begin) + slide;
    if (loaded_begin < 0) return {{}, "Invalid Mach-O image slide: " + name};
    return Result<std::pair<std::uint64_t, std::uint64_t>>(
        {static_cast<std::uint64_t>(loaded_begin), end - begin});
  }
  return {{}, "Module not found: " + name};
}
#endif

#if LR_PLATFORM_WINDOWS
Result<HMODULE> find_module(const std::string& name) {
  std::vector<HMODULE> modules(256);
  DWORD bytes_needed = 0;
  // Use K32* exports directly — psapi macros are stripped in windows_h.hpp.
  if (!::K32EnumProcessModules(::GetCurrentProcess(), modules.data(),
                            static_cast<DWORD>(modules.size() * sizeof(HMODULE)), &bytes_needed)) {
    return {{}, os_error("EnumProcessModules").error_msg};
  }
  if (bytes_needed > modules.size() * sizeof(HMODULE)) {
    modules.resize((bytes_needed + sizeof(HMODULE) - 1) / sizeof(HMODULE));
    if (!::K32EnumProcessModules(::GetCurrentProcess(), modules.data(),
                              static_cast<DWORD>(modules.size() * sizeof(HMODULE)), &bytes_needed)) {
      return {{}, os_error("EnumProcessModules").error_msg};
    }
  }
  for (std::size_t index = 0; index < bytes_needed / sizeof(HMODULE); ++index) {
    char path[MAX_PATH]{};
    if (::K32GetModuleFileNameExA(::GetCurrentProcess(), modules[index], path, MAX_PATH) == 0) continue;
    if (module_name_matches(path, name)) return Result<HMODULE>(modules[index]);
  }
  return {{}, "Module not found: " + name};
}
#endif

}  // namespace

Result<LibHandle> load_library(const std::string& path) {
  if (path.empty()) return {{}, "Library path is empty"};
#if LR_PLATFORM_WINDOWS
  auto& api = win::g_Api();
  HMODULE module = api.LoadLibraryA(path.c_str());
  if (module == nullptr) return {{}, os_error("LoadLibraryA").error_msg};
  return Result<LibHandle>({module});
#elif LR_PLATFORM_LINUX || defined(__APPLE__)
  void* module = ::dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (module == nullptr) {
    const char* error = ::dlerror();
    return {{}, std::string("dlopen: ") + (error != nullptr ? error : "unknown error")};
  }
  return Result<LibHandle>({module});
#else
  return {{}, "Dynamic libraries are unsupported on this platform"};
#endif
}

Result<LibHandle> load_system_library(const std::string& name) {
  if (name.empty()) return {{}, "Library name is empty"};
#if LR_PLATFORM_WINDOWS
  char system_directory[MAX_PATH]{};
  const UINT length = ::GetSystemDirectoryA(system_directory, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) return {{}, os_error("GetSystemDirectoryA").error_msg};
  return load_library(std::string(system_directory) + "\\" + name);
#elif LR_PLATFORM_LINUX || defined(__APPLE__)
  void* module = ::dlopen(name.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (module == nullptr) {
    const char* error = ::dlerror();
    return {{}, std::string("dlopen: ") + (error != nullptr ? error : "unknown error")};
  }
  return Result<LibHandle>({module});
#else
  return {{}, "Dynamic libraries are unsupported on this platform"};
#endif
}

Result<void*> get_symbol(LibHandle lib, const std::string& name) {
  if (lib.native == nullptr || name.empty()) return {{}, "Invalid library handle or symbol name"};
#if LR_PLATFORM_WINDOWS
  auto& api = win::g_Api();
  void* symbol = api.GetProcAddress(static_cast<HMODULE>(lib.native), name.c_str());
  if (symbol == nullptr) return {{}, os_error("GetProcAddress").error_msg};
  return Result<void*>(symbol);
#elif LR_PLATFORM_LINUX || defined(__APPLE__)
  ::dlerror();
  void* symbol = ::dlsym(lib.native, name.c_str());
  const char* error = ::dlerror();
  if (error != nullptr) return {{}, std::string("dlsym: ") + error};
  return Result<void*>(symbol);
#else
  return {{}, "Dynamic libraries are unsupported on this platform"};
#endif
}

Result<void> unload_library(LibHandle lib) {
  if (lib.native == nullptr) return Result<void>("Invalid library handle");
#if LR_PLATFORM_WINDOWS
  if (!::FreeLibrary(static_cast<HMODULE>(lib.native))) return os_error("FreeLibrary");
  return {};
#elif LR_PLATFORM_LINUX || defined(__APPLE__)
  if (::dlclose(lib.native) != 0) {
    const char* error = ::dlerror();
    return Result<void>(std::string("dlclose: ") + (error != nullptr ? error : "unknown error"));
  }
  return {};
#else
  return Result<void>("Dynamic libraries are unsupported on this platform");
#endif
}

Result<std::uint64_t> get_module_base(const std::string& name) {
#if LR_PLATFORM_WINDOWS
  auto module = find_module(name);
  if (!module) return {{}, module.error_msg};
  return Result<std::uint64_t>(reinterpret_cast<std::uint64_t>(*module));
#elif LR_PLATFORM_LINUX
  auto range = module_range(name);
  if (!range) return {{}, range.error_msg};
  return Result<std::uint64_t>((*range).first);
#elif defined(__APPLE__)
  auto range = module_range(name);
  if (!range) return {{}, range.error_msg};
  return Result<std::uint64_t>((*range).first);
#else
  return {{}, "Module lookup is unsupported on this platform"};
#endif
}

Result<std::size_t> get_module_size(const std::string& name) {
#if LR_PLATFORM_WINDOWS
  auto module = find_module(name);
  if (!module) return {{}, module.error_msg};
  MODULEINFO info{};
  if (!::K32GetModuleInformation(::GetCurrentProcess(), *module, &info, sizeof(info))) {
    return {{}, os_error("GetModuleInformation").error_msg};
  }
  return Result<std::size_t>(info.SizeOfImage);
#elif LR_PLATFORM_LINUX
  auto range = module_range(name);
  if (!range) return {{}, range.error_msg};
  return Result<std::size_t>(static_cast<std::size_t>((*range).second - (*range).first));
#elif defined(__APPLE__)
  auto range = module_range(name);
  if (!range) return {{}, range.error_msg};
  return Result<std::size_t>(static_cast<std::size_t>((*range).second));
#else
  return {{}, "Module lookup is unsupported on this platform"};
#endif
}

Result<std::vector<std::uint8_t>> read_module_memory(const std::string& name,
                                                      std::uint64_t offset,
                                                      std::size_t size) {
  auto base = get_module_base(name);
  if (!base) return {{}, base.error_msg};
  auto module_size = get_module_size(name);
  if (!module_size) return {{}, module_size.error_msg};
  if (offset > *module_size || size > *module_size - offset) {
    return {{}, "Requested range is outside the module"};
  }
  std::vector<std::uint8_t> result(size);
  std::memcpy(result.data(), reinterpret_cast<const void*>(*base + offset), size);
  return Result<std::vector<std::uint8_t>>(std::move(result));
}

}  // namespace real
